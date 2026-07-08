"""CodexMeter macOS daemon."""

from __future__ import annotations

import argparse
import asyncio
import logging
import signal
import sys
import time

from .ble import BleState, BleTransport
from .events import EventServer
from .payloads import Payload, build_activity_payload, build_usage_payload
from .provider import CodexUsageProvider
from .screen_policy import screen_policy_loop
from .settings import (
    APP_DIR,
    AUTO_SCREEN_TIMEOUT_SEC,
    BLE_ACK_TIMEOUT_SEC,
    BLE_HEALTHCHECK_INTERVAL_SEC,
    BLE_NOTIFY_TIMEOUT_SEC,
    BLE_WRITE_TIMEOUT_SEC,
    LOCK_POLL_INTERVAL_SEC,
    LOG_FILE,
    POLL_INTERVAL_SEC,
)

COALESCED_PAYLOAD_KINDS = {"usage", "activity", "control"}


async def quota_loop(
    provider: CodexUsageProvider,
    queue: "asyncio.Queue[Payload]",
    stop_event: asyncio.Event,
    poll_interval: int,
) -> None:
    last_usage_payload: Payload | None = None
    while not stop_event.is_set():
        try:
            snapshot = await provider.fetch()
            payload = build_usage_payload(snapshot)
            last_usage_payload = payload
            await put_latest(queue, payload)
            logging.info(
                "Queued usage h5=%s d7=%s status=%s",
                snapshot.h5_remaining_percent,
                snapshot.d7_remaining_percent,
                snapshot.status,
            )
        except asyncio.CancelledError:
            raise
        except Exception:
            logging.exception("Failed to fetch Codex usage")
            if last_usage_payload is not None:
                stale_payload = build_stale_usage_payload(last_usage_payload)
                await put_latest(queue, stale_payload)
                logging.info("Queued stale usage heartbeat after fetch failure")

        try:
            await asyncio.wait_for(stop_event.wait(), timeout=poll_interval)
        except asyncio.TimeoutError:
            pass


def build_stale_usage_payload(payload: Payload, now: int | None = None) -> Payload:
    data = dict(payload.data)
    data["st"] = "stale"
    data["t"] = int(time.time() if now is None else now)
    return Payload("usage", data)


async def put_latest(queue: "asyncio.Queue[Payload]", payload: Payload) -> None:
    if payload.kind in COALESCED_PAYLOAD_KINDS:
        kept: list[Payload] = []
        while True:
            try:
                existing = queue.get_nowait()
            except asyncio.QueueEmpty:
                break
            if existing.kind != payload.kind:
                kept.append(existing)
        for existing in kept:
            await queue.put(existing)

    if queue.full():
        _ = queue.get_nowait()
    await queue.put(payload)


async def run_daemon(args: argparse.Namespace) -> None:
    queue: asyncio.Queue[Payload] = asyncio.Queue(maxsize=32)
    stop_event = asyncio.Event()
    loop = asyncio.get_running_loop()

    def stop() -> None:
        logging.info("Stopping CodexMeter daemon")
        stop_event.set()

    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop)
        except NotImplementedError:
            signal.signal(sig, lambda *_: stop())

    async def sink(payload: Payload) -> None:
        await put_latest(queue, payload)

    provider = CodexUsageProvider(
        codex_bin=args.codex_bin,
        timeout_sec=args.timeout,
        refresh_token=args.refresh_token,
    )
    ble_state = BleState()
    ble_state.last_activity = build_activity_payload(0)
    transport = BleTransport(
        device_name=args.device_name,
        scan_timeout_sec=args.scan_timeout,
        write_timeout_sec=args.ble_write_timeout,
        ack_timeout_sec=args.ble_ack_timeout,
        notify_timeout_sec=args.ble_notify_timeout,
        healthcheck_interval_sec=args.ble_healthcheck_interval,
    )

    def status_provider() -> dict[str, object]:
        return {"ble": ble_state.status(queue_depth=queue.qsize())}

    event_server = EventServer(sink, status_provider=status_provider)

    tasks = [
        asyncio.create_task(event_server.run(stop_event), name="events"),
        asyncio.create_task(
            quota_loop(provider, queue, stop_event, args.poll_interval), name="quota"
        ),
        asyncio.create_task(
            screen_policy_loop(
                sink,
                ble_state,
                stop_event,
                timeout_sec=args.auto_screen_timeout,
                poll_interval_sec=args.lock_poll_interval,
            ),
            name="screen_policy",
        ),
        asyncio.create_task(transport.run(queue, ble_state, stop_event), name="ble"),
    ]

    await stop_event.wait()
    for task in tasks:
        task.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)


def configure_logging(level: str, foreground: bool) -> None:
    APP_DIR.mkdir(parents=True, exist_ok=True)
    handlers: list[logging.Handler] = [logging.FileHandler(LOG_FILE)]
    if foreground:
        handlers.append(logging.StreamHandler())
    logging.basicConfig(
        level=getattr(logging, level.upper(), logging.INFO),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        handlers=handlers,
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the CodexMeter host daemon.")
    parser.add_argument("--codex-bin", default="codex")
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--poll-interval", type=int, default=POLL_INTERVAL_SEC)
    parser.add_argument("--scan-timeout", type=float, default=8.0)
    parser.add_argument("--ble-write-timeout", type=float, default=BLE_WRITE_TIMEOUT_SEC)
    parser.add_argument("--ble-ack-timeout", type=float, default=BLE_ACK_TIMEOUT_SEC)
    parser.add_argument("--ble-notify-timeout", type=float, default=BLE_NOTIFY_TIMEOUT_SEC)
    parser.add_argument(
        "--ble-healthcheck-interval",
        type=float,
        default=BLE_HEALTHCHECK_INTERVAL_SEC,
    )
    parser.add_argument("--auto-screen-timeout", type=float, default=AUTO_SCREEN_TIMEOUT_SEC)
    parser.add_argument("--lock-poll-interval", type=float, default=LOCK_POLL_INTERVAL_SEC)
    parser.add_argument("--device-name", default="CodexMeter")
    parser.add_argument("--refresh-token", action="store_true")
    parser.add_argument("--log-level", default="INFO")
    parser.add_argument("--foreground", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    configure_logging(args.log_level, foreground=args.foreground)
    try:
        asyncio.run(run_daemon(args))
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
