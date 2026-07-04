#!/usr/bin/env python3
"""Capture a CodexMeter framebuffer over USB serial and write a PNG."""

from __future__ import annotations

import argparse
import glob
import os
import select
import struct
import sys
import termios
import time
import zlib
from pathlib import Path

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
DEFAULT_LINE_TIMEOUT_SECONDS = 8.0
DEFAULT_RAW_TIMEOUT_SECONDS = 30.0


class CaptureError(RuntimeError):
    """Raised when the device does not return a valid screenshot frame."""


class SerialCapture:
    def __init__(self, port: str) -> None:
        self.port = port
        self.fd: int | None = None
        self.buffer = bytearray()

    def __enter__(self) -> "SerialCapture":
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self._configure_port()
        termios.tcflush(self.fd, termios.TCIFLUSH)
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def _configure_port(self) -> None:
        if self.fd is None:
            raise CaptureError("serial port is not open")
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] |= termios.CLOCAL | termios.CREAD
        attrs[2] &= ~termios.PARENB
        attrs[2] &= ~termios.CSTOPB
        attrs[2] &= ~termios.CSIZE
        attrs[2] |= termios.CS8
        attrs[3] = 0
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)

    def write_command(self, command: bytes) -> None:
        if self.fd is None:
            raise CaptureError("serial port is not open")
        total = 0
        while total < len(command):
            total += os.write(self.fd, command[total:])

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
            raise CaptureError("serial port is not open")
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise CaptureError("timed out waiting for screenshot data")
        ready, _, _ = select.select([self.fd], [], [], min(0.25, remaining))
        if not ready:
            return
        try:
            chunk = os.read(self.fd, 8192)
        except BlockingIOError:
            return
        if chunk:
            self.buffer.extend(chunk)


def detect_port() -> str:
    patterns = [
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/ttyACM*",
        "/dev/ttyUSB*",
    ]
    ports: list[str] = []
    for pattern in patterns:
        ports.extend(sorted(glob.glob(pattern)))
    if not ports:
        raise CaptureError("no USB serial port found; pass the port path explicitly")
    return sorted(dict.fromkeys(ports), key=_port_score)[0]


def _port_score(port: str) -> tuple[int, str]:
    name = os.path.basename(port)
    score = 0
    if "000000000000" in name:
        score += 20
    if "usbserial" in name:
        score += 10
    if name.startswith("tty"):
        score += 5
    return score, name


def capture_rgb565le(
    port: str,
    *,
    line_timeout: float = DEFAULT_LINE_TIMEOUT_SECONDS,
    raw_timeout: float = DEFAULT_RAW_TIMEOUT_SECONDS,
) -> tuple[int, int, bytes]:
    with SerialCapture(port) as serial:
        serial.write_command(b"screenshot\n")
        while True:
            line = serial.read_line(line_timeout)
            text = line.decode("utf-8", errors="replace").strip()
            if not text:
                continue
            if text == "SCREENSHOT_ERR":
                raise CaptureError("device reported screenshot error")
            if text == "SCREENSHOT_UNSUPPORTED":
                raise CaptureError("device does not support full-frame screenshots")
            if not text.startswith("SCREENSHOT_START "):
                continue

            parts = text.split()
            if len(parts) != 4:
                raise CaptureError(f"invalid screenshot header: {text}")
            width, height, raw_size = (int(parts[1]), int(parts[2]), int(parts[3]))
            expected_size = width * height * 2
            if raw_size != expected_size:
                raise CaptureError(
                    f"invalid raw size {raw_size}; expected {expected_size} for {width}x{height}"
                )
            data = serial.read_exact(raw_size, raw_timeout)
            _wait_for_end(serial, line_timeout)
            return width, height, data


def _wait_for_end(serial: SerialCapture, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            line = serial.read_line(max(0.01, deadline - time.monotonic()))
        except CaptureError:
            break
        if line.decode("utf-8", errors="replace").strip() == "SCREENSHOT_END":
            return


def rgb565le_to_png(width: int, height: int, data: bytes) -> bytes:
    if len(data) != width * height * 2:
        raise ValueError("RGB565 data length does not match dimensions")

    rows = bytearray()
    offset = 0
    for _y in range(height):
        rows.append(0)
        for _x in range(width):
            value = data[offset] | (data[offset + 1] << 8)
            offset += 2
            r5 = (value >> 11) & 0x1F
            g6 = (value >> 5) & 0x3F
            b5 = value & 0x1F
            rows.append((r5 << 3) | (r5 >> 2))
            rows.append((g6 << 2) | (g6 >> 4))
            rows.append((b5 << 3) | (b5 >> 2))

    return (
        PNG_SIGNATURE
        + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + _png_chunk(b"IDAT", zlib.compress(bytes(rows), level=6))
        + _png_chunk(b"IEND", b"")
    )


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(kind)
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", crc)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture a CodexMeter USB serial screenshot as a PNG."
    )
    parser.add_argument("output", nargs="?", default="screenshot.png", help="PNG output path")
    parser.add_argument("port", nargs="?", help="USB serial port, e.g. /dev/cu.usbmodem211201")
    parser.add_argument(
        "--line-timeout",
        type=float,
        default=DEFAULT_LINE_TIMEOUT_SECONDS,
        help="seconds to wait for serial status lines",
    )
    parser.add_argument(
        "--raw-timeout",
        type=float,
        default=DEFAULT_RAW_TIMEOUT_SECONDS,
        help="seconds to wait for raw framebuffer bytes",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        port = args.port or detect_port()
        width, height, data = capture_rgb565le(
            port,
            line_timeout=args.line_timeout,
            raw_timeout=args.raw_timeout,
        )
        output = Path(args.output)
        output.write_bytes(rgb565le_to_png(width, height, data))
    except (CaptureError, OSError, ValueError) as exc:
        print(f"Screenshot capture failed: {exc}", file=sys.stderr)
        return 1

    print(f"Captured {width}x{height} ({len(data)} bytes) from {port}")
    print(f"Saved: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
