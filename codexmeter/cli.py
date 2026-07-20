"""Command-line helpers for development and installation checks."""

from __future__ import annotations

import argparse
import asyncio
import json
import sys
import time

from .device_registry import (
    DeviceRegistry,
    config_from_short_id,
    normalize_short_id,
)
from .events import send_event
from .multi_ble import scan_codexmeter_devices
from .payloads import build_usage_payload
from .provider import CodexUsageProvider


async def run_once(args: argparse.Namespace) -> int:
    provider = CodexUsageProvider(args.codex_bin, args.timeout, args.refresh_token)
    snapshot = await provider.fetch()
    payload = build_usage_payload(snapshot)
    print(json.dumps(payload.data, ensure_ascii=False, indent=2))
    return 0


async def run_status(_args: argparse.Namespace) -> int:
    result = await send_event({"type": "ping", "include_activity_tasks": True})
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
                "h5": None if args.no_h5 else args.h5,
                "h5r": None if args.no_h5 else now + 73 * 60,
                "d7": args.d7,
                "d7r": now + 3 * 24 * 60 * 60,
                "td": args.today_tokens,
                "t7": args.week_tokens,
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


async def run_devices_scan(args: argparse.Namespace) -> int:
    devices = await scan_codexmeter_devices(timeout=args.timeout)
    print(
        json.dumps(
            [
                {
                    "name": device.name,
                    "short_id": device.short_id,
                    "address": device.address,
                }
                for device in devices
            ],
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


async def run_devices_list(_args: argparse.Namespace) -> int:
    registry = DeviceRegistry.load()
    print(
        json.dumps(
            [device.to_json() for device in registry.devices],
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


async def run_devices_adopt(args: argparse.Namespace) -> int:
    registry = DeviceRegistry.load()
    device = config_from_short_id(args.short_id, alias=args.alias)
    registry.upsert(device)
    registry.save()
    print(json.dumps(device.to_json(), ensure_ascii=False, indent=2))
    return 0


async def run_devices_rename(args: argparse.Namespace) -> int:
    registry = DeviceRegistry.load()
    device = registry.find(args.device)
    if device is None:
        print(f"Error: unknown device {args.device}", file=sys.stderr)
        return 1
    updated = type(device)(
        device_id=device.device_id,
        short_id=device.short_id,
        alias=args.alias,
        enabled=device.enabled,
        name=device.name,
        macos_uuid=device.macos_uuid,
        legacy=device.legacy,
    )
    registry.upsert(updated)
    registry.save()
    print(json.dumps(updated.to_json(), ensure_ascii=False, indent=2))
    return 0


async def run_devices_enabled(args: argparse.Namespace) -> int:
    registry = DeviceRegistry.load()
    device = registry.find(args.device)
    if device is None:
        print(f"Error: unknown device {args.device}", file=sys.stderr)
        return 1
    updated = type(device)(
        device_id=device.device_id,
        short_id=device.short_id,
        alias=device.alias,
        enabled=args.enabled,
        name=device.name,
        macos_uuid=device.macos_uuid,
        legacy=device.legacy,
    )
    registry.upsert(updated)
    registry.save()
    print(json.dumps(updated.to_json(), ensure_ascii=False, indent=2))
    return 0


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
    usage.add_argument(
        "--no-h5",
        action="store_true",
        help="Hide the 5h window and preview the token-activity layout.",
    )
    usage.add_argument("--today-tokens", type=int, default=18_600_000)
    usage.add_argument("--week-tokens", type=int, default=236_000_000)
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

    devices = sub.add_parser("devices", help="Manage registered CodexMeter devices.")
    device_sub = devices.add_subparsers(dest="device_cmd", required=True)

    devices_scan = device_sub.add_parser("scan", help="Scan nearby CodexMeter BLE devices.")
    devices_scan.add_argument("--timeout", type=float, default=4.0)
    devices_scan.set_defaults(func=run_devices_scan)

    devices_list = device_sub.add_parser("list", help="List registered devices.")
    devices_list.set_defaults(func=run_devices_list)

    devices_adopt = device_sub.add_parser("adopt", help="Register a nearby device short id.")
    devices_adopt.add_argument("short_id", type=normalize_short_id)
    devices_adopt.add_argument("--alias")
    devices_adopt.set_defaults(func=run_devices_adopt)

    devices_rename = device_sub.add_parser("rename", help="Set a local alias for a device.")
    devices_rename.add_argument("device")
    devices_rename.add_argument("alias")
    devices_rename.set_defaults(func=run_devices_rename)

    devices_enable = device_sub.add_parser("enable", help="Enable auto-connect for a device.")
    devices_enable.add_argument("device")
    devices_enable.set_defaults(func=run_devices_enabled, enabled=True)

    devices_disable = device_sub.add_parser("disable", help="Disable auto-connect for a device.")
    devices_disable.add_argument("device")
    devices_disable.set_defaults(func=run_devices_enabled, enabled=False)

    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return asyncio.run(args.func(args))
    except (OSError, TimeoutError, RuntimeError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
