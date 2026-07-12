#!/usr/bin/env python3
"""Shared USB serial helpers for CodexMeter host-side tools."""

from __future__ import annotations

import glob
import json
import os
import select
import termios
import time
from dataclasses import dataclass

DEFAULT_BAUD = termios.B115200
DEFAULT_IDENTITY_TIMEOUT = 2.0


class SerialToolError(RuntimeError):
    """Raised when a USB serial command cannot complete."""


@dataclass(frozen=True)
class SerialDeviceIdentity:
    port: str
    device_id: str | None
    short_id: str | None
    name: str | None

    def matches(self, query: str) -> bool:
        text = query.strip()
        return text in {
            self.port,
            os.path.basename(self.port),
            self.device_id or "",
            self.short_id or "",
            self.name or "",
        }


class SerialSession:
    def __init__(self, port: str, *, flush_input: bool = True) -> None:
        self.port = port
        self.flush_input = flush_input
        self.fd: int | None = None
        self.buffer = bytearray()

    def __enter__(self) -> "SerialSession":
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self._configure_port()
        if self.flush_input:
            termios.tcflush(self.fd, termios.TCIFLUSH)
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def _configure_port(self) -> None:
        if self.fd is None:
            raise SerialToolError("serial port is not open")
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] |= termios.CLOCAL | termios.CREAD
        attrs[2] &= ~termios.PARENB
        attrs[2] &= ~termios.CSTOPB
        attrs[2] &= ~termios.CSIZE
        attrs[2] |= termios.CS8
        attrs[3] = 0
        attrs[4] = DEFAULT_BAUD
        attrs[5] = DEFAULT_BAUD
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)

    def write_command(self, command: bytes) -> None:
        if self.fd is None:
            raise SerialToolError("serial port is not open")
        total = 0
        while total < len(command):
            total += os.write(self.fd, command[total:])

    def write_line(self, line: str) -> None:
        self.write_command(line.encode("utf-8") + b"\n")

    def read_line(self, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while True:
            newline = self.buffer.find(b"\n")
            if newline >= 0:
                line = bytes(self.buffer[:newline])
                del self.buffer[: newline + 1]
                return line.rstrip(b"\r")
            self._read_more(deadline)

    def read_exact(self, size: int, timeout: float) -> bytes:
        deadline = time.monotonic() + timeout
        while len(self.buffer) < size:
            self._read_more(deadline)
        data = bytes(self.buffer[:size])
        del self.buffer[:size]
        return data

    def _read_more(self, deadline: float) -> None:
        if self.fd is None:
            raise SerialToolError("serial port is not open")
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise SerialToolError("timed out waiting for serial data")
        ready, _, _ = select.select([self.fd], [], [], min(0.25, remaining))
        if not ready:
            return
        try:
            chunk = os.read(self.fd, 8192)
        except BlockingIOError:
            return
        if chunk:
            self.buffer.extend(chunk)


def list_ports() -> list[str]:
    patterns = [
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/ttyACM*",
        "/dev/ttyUSB*",
    ]
    ports: list[str] = []
    for pattern in patterns:
        ports.extend(sorted(glob.glob(pattern)))
    return sorted(dict.fromkeys(ports), key=port_score)


def detect_port() -> str:
    ports = list_ports()
    if not ports:
        raise SerialToolError("no USB serial port found; pass the port path explicitly")
    return ports[0]


def query_identity(
    port: str,
    *,
    timeout: float = DEFAULT_IDENTITY_TIMEOUT,
) -> SerialDeviceIdentity | None:
    try:
        with SerialSession(port) as serial:
            serial.write_line("identity")
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                line = serial.read_line(max(0.05, deadline - time.monotonic()))
                text = line.decode("utf-8", errors="replace").strip()
                if text.startswith("IDENTITY "):
                    data = json.loads(text[len("IDENTITY ") :])
                    if not isinstance(data, dict):
                        return None
                    return SerialDeviceIdentity(
                        port=port,
                        device_id=_optional_str(data.get("device_id")),
                        short_id=_optional_str(data.get("short_id")),
                        name=_optional_str(data.get("name")),
                    )
    except (OSError, SerialToolError, json.JSONDecodeError):
        return None
    return None


def discover_serial_devices(
    *,
    timeout: float = DEFAULT_IDENTITY_TIMEOUT,
) -> list[SerialDeviceIdentity]:
    devices: list[SerialDeviceIdentity] = []
    for port in list_ports():
        identity = query_identity(port, timeout=timeout)
        if identity is not None:
            devices.append(identity)
    return devices


def resolve_port(
    port: str | None = None,
    *,
    device: str | None = None,
    timeout: float = DEFAULT_IDENTITY_TIMEOUT,
) -> str:
    if port:
        return port
    if device:
        matches = [
            identity
            for identity in discover_serial_devices(timeout=timeout)
            if identity.matches(device)
        ]
        if len(matches) == 1:
            return matches[0].port
        if matches:
            ports = ", ".join(identity.port for identity in matches)
            raise SerialToolError(f"multiple serial devices match {device!r}: {ports}")
        raise SerialToolError(f"no serial device matches {device!r}")

    devices = discover_serial_devices(timeout=timeout)
    if len(devices) == 1:
        return devices[0].port
    if len(devices) > 1:
        summary = ", ".join(
            f"{identity.short_id or '?'}={identity.port}" for identity in devices
        )
        raise SerialToolError(
            "multiple CodexMeter serial devices found; pass --device or a port: "
            + summary
        )

    ports = list_ports()
    if len(ports) == 1:
        return ports[0]
    if not ports:
        raise SerialToolError("no USB serial port found; pass the port path explicitly")
    raise SerialToolError("multiple USB serial ports found; pass --device or a port")


def port_score(port: str) -> tuple[int, str]:
    name = os.path.basename(port)
    score = 0
    if "000000000000" in name:
        score += 20
    if "usbserial" in name:
        score += 10
    if name.startswith("tty"):
        score += 5
    return score, name


def _optional_str(value: object) -> str | None:
    if isinstance(value, str) and value.strip():
        return value.strip()
    return None
