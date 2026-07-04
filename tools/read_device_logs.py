#!/usr/bin/env python3
"""Read CodexMeter ESP32 logs over USB serial."""

from __future__ import annotations

import argparse
import sys

from serial_device import SerialSession, SerialToolError, detect_port

DEFAULT_TIMEOUT_SECONDS = 6.0


def build_logs_command(lines: int = 0) -> str:
    return f"logs {lines}" if lines > 0 else "logs"


def read_logs(port: str, *, lines: int = 0, timeout: float = DEFAULT_TIMEOUT_SECONDS) -> list[str]:
    result: list[str] = []
    with SerialSession(port) as serial:
        serial.write_line(build_logs_command(lines))
        in_block = False
        while True:
            line = serial.read_line(timeout).decode("utf-8", errors="replace").strip()
            if not in_block:
                if line.startswith("LOGS_START "):
                    in_block = True
                    result.append(line)
                continue
            result.append(line)
            if line == "LOGS_END":
                return result


def clear_logs(port: str, *, timeout: float = DEFAULT_TIMEOUT_SECONDS) -> None:
    with SerialSession(port) as serial:
        serial.write_line("log_clear")
        while True:
            line = serial.read_line(timeout).decode("utf-8", errors="replace").strip()
            if line == "LOGS_CLEARED":
                return


def follow_logs(port: str, *, timeout: float = 3600.0) -> None:
    with SerialSession(port, flush_input=False) as serial:
        while True:
            line = serial.read_line(timeout).decode("utf-8", errors="replace")
            print(line, flush=True)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Read CodexMeter ESP32 logs over USB serial.")
    parser.add_argument("port", nargs="?", help="USB serial port, e.g. /dev/cu.usbmodem211201")
    parser.add_argument("-n", "--lines", type=int, default=0, help="limit queried log lines")
    parser.add_argument("--clear", action="store_true", help="clear the device log buffer")
    parser.add_argument("--follow", action="store_true", help="print live serial log lines")
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT_SECONDS,
        help="seconds to wait for query responses",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        port = args.port or detect_port()
        if args.clear:
            clear_logs(port, timeout=args.timeout)
            print(f"Cleared device logs on {port}")
        if args.follow:
            print(f"Following device logs on {port}; press Ctrl-C to stop.", file=sys.stderr)
            follow_logs(port)
            return 0
        for line in read_logs(port, lines=args.lines, timeout=args.timeout):
            print(line)
        return 0
    except KeyboardInterrupt:
        return 130
    except (SerialToolError, OSError) as exc:
        print(f"Read logs failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
