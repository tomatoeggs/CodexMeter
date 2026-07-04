"""CodexMeter macOS daemon."""

from __future__ import annotations

import argparse
import asyncio
import logging
import signal
import sys

from .ble import BleState, BleTransport
from .events import EventServer
from .payloads import Payload, build_usage_payload
from .provider import CodexUsageProvider
from .settings import APP_DIR, LOG_FILE, POLL_INTERVAL_SEC


async def quota_loop(
    provider: CodexUsageProvider,
    queue: "asyncio.Queue[Payload]",
    stop_event: asyncio.Event,
    poll_interval: int,
) -> None:
    while not stop_event.is_set():
        try:
            snapshot = await provider.fetch()
            payload = build_usage_payload(snapshot)
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

        try:
            await asyncio.wait_for(stop_event.wait(), timeout=poll_interval)
        except asyncio.TimeoutError:
            pass


async def put_latest(queue: "asyncio.Queue[Payload]", payload: Payload) -> None:
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
    transport = BleTransport(device_name=args.device_name, scan_timeout_sec=args.scan_timeout)
    event_server = EventServer(sink)

    tasks = [
        asyncio.create_task(event_server.run(stop_event), name="events"),
        asyncio.create_task(
            quota_loop(provider, queue, stop_event, args.poll_interval), name="quota"
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
