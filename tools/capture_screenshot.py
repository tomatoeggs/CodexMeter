#!/usr/bin/env python3
"""Capture a CodexMeter framebuffer over USB serial and write a PNG."""

from __future__ import annotations

import argparse
import struct
import sys
import time
import zlib
from pathlib import Path

from serial_device import (
    SerialSession,
    SerialToolError,
    discover_serial_devices,
    port_score,
    resolve_port,
)

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
DEFAULT_LINE_TIMEOUT_SECONDS = 8.0
DEFAULT_RAW_TIMEOUT_SECONDS = 30.0


class CaptureError(SerialToolError):
    """Raised when the device does not return a valid screenshot frame."""


def capture_rgb565le(
    port: str,
    *,
    line_timeout: float = DEFAULT_LINE_TIMEOUT_SECONDS,
    raw_timeout: float = DEFAULT_RAW_TIMEOUT_SECONDS,
) -> tuple[int, int, bytes]:
    with SerialSession(port) as serial:
        serial.write_line("screenshot")
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


def _wait_for_end(serial: SerialSession, timeout: float) -> None:
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
    parser.add_argument("--device", help="CodexMeter short id, device id, name, or port basename")
    parser.add_argument("--list", action="store_true", help="list identified CodexMeter serial devices")
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
        if args.list:
            for identity in discover_serial_devices(timeout=args.line_timeout):
                print(
                    f"{identity.short_id or '?'} {identity.port} "
                    f"{identity.device_id or '-'} {identity.name or '-'}"
                )
            return 0
        port = resolve_port(args.port, device=args.device, timeout=args.line_timeout)
        width, height, data = capture_rgb565le(
            port,
            line_timeout=args.line_timeout,
            raw_timeout=args.raw_timeout,
        )
        output = Path(args.output)
        output.write_bytes(rgb565le_to_png(width, height, data))
    except (SerialToolError, OSError, ValueError) as exc:
        print(f"Screenshot capture failed: {exc}", file=sys.stderr)
        return 1

    print(f"Captured {width}x{height} ({len(data)} bytes) from {port}")
    print(f"Saved: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
