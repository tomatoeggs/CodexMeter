"""Command-line helpers for development and installation checks."""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
import time

from .events import send_event
from .payloads import build_usage_payload
from .provider import CodexUsageProvider


async def run_once(args: argparse.Namespace) -> int:
    provider = CodexUsageProvider(args.codex_bin, args.timeout, args.refresh_token)
    snapshot = await provider.fetch()
    payload = build_usage_payload(snapshot)
    print(json.dumps(payload.data, ensure_ascii=False, indent=2))
    return 0


async def run_status(_args: argparse.Namespace) -> int:
    result = await send_event({"type": "ping"})
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


async def run_demo_alert(args: argparse.Namespace) -> int:
    result = await send_event(
        {"type": "alert", "title": "任务完成！", "body": args.message}
    )
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


async def run_demo_usage(args: argparse.Namespace) -> int:
    now = int(time.time())
    result = await send_event(
        {
            "type": "usage",
            "payload": {
                "v": 1,
                "k": "usage",
                "src": "codex",
                "h5": args.h5,
                "h5r": now + 73 * 60,
                "d7": args.d7,
                "d7r": now + 3 * 24 * 60 * 60,
                "st": "ok",
                "t": now,
            },
        }
    )
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


async def run_demo_activity(args: argparse.Namespace) -> int:
    result = await send_event({"type": "activity", "count": args.count})
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


async def run_screen(args: argparse.Namespace) -> int:
    result = await send_event(
        {"type": "screen", "on": args.on, "reason": args.reason}
    )
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="CodexMeter helper CLI.")
    sub = parser.add_subparsers(dest="cmd", required=True)

    once = sub.add_parser("once", help="Fetch Codex usage once and print BLE JSON.")
    once.add_argument("--codex-bin", default="codex")
    once.add_argument("--timeout", type=float, default=15.0)
    once.add_argument("--refresh-token", action="store_true")
    once.set_defaults(func=run_once)

    status = sub.add_parser("status", help="Ping the local daemon event socket.")
    status.set_defaults(func=run_status)

    alert = sub.add_parser("demo-alert", help="Queue a demo task-complete alert.")
    alert.add_argument("message", nargs="?", default="Codex 已完成一个测试任务")
    alert.set_defaults(func=run_demo_alert)

    usage = sub.add_parser("demo-usage", help="Queue a demo usage payload.")
    usage.add_argument("--h5", type=int, default=72)
    usage.add_argument("--d7", type=int, default=84)
    usage.set_defaults(func=run_demo_usage)

    activity = sub.add_parser("demo-activity", help="Queue a demo running-task count.")
    activity.add_argument("--count", type=int, default=1)
    activity.set_defaults(func=run_demo_activity)

    screen_on = sub.add_parser("screen-on", help="Queue a screen-on control payload.")
    screen_on.add_argument("--reason", default="manual")
    screen_on.set_defaults(func=run_screen, on=True)

    screen_off = sub.add_parser("screen-off", help="Queue a screen-off control payload.")
    screen_off.add_argument("--reason", default="manual")
    screen_off.set_defaults(func=run_screen, on=False)

    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return asyncio.run(args.func(args))
    except (OSError, TimeoutError, RuntimeError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
