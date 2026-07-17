"""CodexMeter macOS daemon."""

from __future__ import annotations

import argparse
import asyncio
import fcntl
import json
import logging
import os
import signal
import sys
import time
from collections.abc import Awaitable, Callable
from logging.handlers import RotatingFileHandler
from pathlib import Path
from typing import TextIO

from .ble import BleIdentity
from .device_registry import DeviceConfig, DeviceRegistry
from .events import EventServer
from .limits import (
    UsageSnapshot,
    UsageSnapshotDecision,
    UsageSnapshotStabilizer,
    is_suspicious_initial_snapshot,
    merge_missing_token_activity,
)
from .multi_ble import DeviceManager
from .payloads import Payload, build_activity_payload, build_usage_payload
from .persistence import atomic_write_json
from .provider import CodexUsageProvider
from .queueing import put_latest
from .screen_policy import screen_policy_loop
from .settings import (
    APP_DIR,
    AUTO_SCREEN_TIMEOUT_SEC,
    BLE_ACK_TIMEOUT_SEC,
    BLE_CONNECT_TIMEOUT_SEC,
    BLE_DISCONNECT_TIMEOUT_SEC,
    BLE_HEALTHCHECK_INTERVAL_SEC,
    BLE_NOTIFY_TIMEOUT_SEC,
    BLE_WRITE_TIMEOUT_SEC,
    DAEMON_LOCK_FILE,
    LOCK_POLL_INTERVAL_SEC,
    LOG_FILE,
    LOG_MAX_TOTAL_BYTES,
    POLL_INTERVAL_SEC,
)
from .supervision import supervise

USAGE_CACHE_FILE = APP_DIR / "last_usage_snapshot.json"
PayloadSink = Callable[[Payload], Awaitable[None]]


class DaemonAlreadyRunning(RuntimeError):
    pass


class DaemonLock:
    def __init__(self, path: Path = DAEMON_LOCK_FILE) -> None:
        self.path = path
        self.handle: TextIO | None = None

    def __enter__(self) -> "DaemonLock":
        self.path.parent.mkdir(parents=True, exist_ok=True)
        handle = self.path.open("a+", encoding="ascii")
        try:
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            handle.close()
            raise DaemonAlreadyRunning("CodexMeter daemon is already running") from exc
        handle.seek(0)
        handle.truncate()
        handle.write(f"{os.getpid()}\n")
        handle.flush()
        self.handle = handle
        return self

    def __exit__(self, *_exc: object) -> None:
        if self.handle is None:
            return
        handle = self.handle
        self.handle = None
        fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
        handle.close()


async def quota_loop(
    provider: CodexUsageProvider,
    sink: PayloadSink,
    stop_event: asyncio.Event,
    poll_interval: int,
) -> None:
    cached_snapshot = load_cached_usage_snapshot()
    if cached_snapshot is not None:
        logging.info(
            "Loaded cached usage h5=%s d7=%s h5r=%s d7r=%s td=%s t7=%s status=%s",
            cached_snapshot.h5_remaining_percent,
            cached_snapshot.d7_remaining_percent,
            cached_snapshot.h5_resets_at,
            cached_snapshot.d7_resets_at,
            cached_snapshot.today_tokens,
            cached_snapshot.last_7d_tokens,
            cached_snapshot.status,
        )
    last_usage_payload = (
        build_usage_payload(cached_snapshot) if cached_snapshot is not None else None
    )
    stabilizer = UsageSnapshotStabilizer(trusted=cached_snapshot)
    while not stop_event.is_set():
        try:
            raw_snapshot = await provider.fetch()
            raw_snapshot = merge_missing_token_activity(
                raw_snapshot,
                stabilizer.trusted,
            )
            if stabilizer.trusted is None and is_suspicious_initial_snapshot(raw_snapshot):
                logging.warning(
                    "Rejected suspicious initial usage sample raw_h5=%s raw_d7=%s "
                    "raw_h5r=%s raw_d7r=%s",
                    raw_snapshot.h5_remaining_percent,
                    raw_snapshot.d7_remaining_percent,
                    raw_snapshot.h5_resets_at,
                    raw_snapshot.d7_resets_at,
                )
            else:
                decision = stabilizer.stabilize(raw_snapshot)
                snapshot = decision.snapshot
                payload = build_usage_payload(snapshot)
                last_usage_payload = payload
                await sink(payload)
                log_usage_decision(raw_snapshot, decision)
                if decision.accepted:
                    save_cached_usage_snapshot(snapshot)
        except asyncio.CancelledError:
            raise
        except Exception:
            logging.exception("Failed to fetch Codex usage")
            if last_usage_payload is not None:
                stale_payload = build_stale_usage_payload(last_usage_payload)
                await sink(stale_payload)
                logging.info("Queued stale usage heartbeat after fetch failure")

        try:
            await asyncio.wait_for(stop_event.wait(), timeout=poll_interval)
        except asyncio.TimeoutError:
            pass


def log_usage_decision(
    raw_snapshot: UsageSnapshot,
    decision: UsageSnapshotDecision,
) -> None:
    snapshot = decision.snapshot
    if decision.accepted:
        logging.info(
            "Queued usage h5=%s d7=%s h5r=%s d7r=%s td=%s t7=%s status=%s decision=%s",
            snapshot.h5_remaining_percent,
            snapshot.d7_remaining_percent,
            snapshot.h5_resets_at,
            snapshot.d7_resets_at,
            snapshot.today_tokens,
            snapshot.last_7d_tokens,
            snapshot.status,
            decision.reason,
        )
        return

    logging.warning(
        "Rejected transient usage sample reason=%s raw_h5=%s raw_d7=%s "
        "raw_h5r=%s raw_d7r=%s queued_h5=%s queued_d7=%s queued_h5r=%s queued_d7r=%s",
        decision.reason,
        raw_snapshot.h5_remaining_percent,
        raw_snapshot.d7_remaining_percent,
        raw_snapshot.h5_resets_at,
        raw_snapshot.d7_resets_at,
        snapshot.h5_remaining_percent,
        snapshot.d7_remaining_percent,
        snapshot.h5_resets_at,
        snapshot.d7_resets_at,
    )


def load_cached_usage_snapshot(
    path: object = USAGE_CACHE_FILE,
) -> UsageSnapshot | None:
    path_obj = Path(path)
    try:
        with path_obj.open(encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, json.JSONDecodeError):
        return None
    if not isinstance(data, dict):
        return None
    snapshot = usage_snapshot_from_cache(data)
    if snapshot is not None and is_suspicious_initial_snapshot(snapshot):
        logging.warning("Ignoring suspicious cached usage snapshot")
        return None
    return snapshot


def save_cached_usage_snapshot(
    snapshot: UsageSnapshot,
    path: object = USAGE_CACHE_FILE,
) -> None:
    path_obj = Path(path)
    try:
        atomic_write_json(path_obj, usage_snapshot_to_cache(snapshot))
    except OSError:
        logging.exception("Failed to save usage cache")


def usage_snapshot_to_cache(snapshot: UsageSnapshot) -> dict[str, object]:
    return {
        "source": snapshot.source,
        "h5_remaining_percent": snapshot.h5_remaining_percent,
        "h5_resets_at": snapshot.h5_resets_at,
        "d7_remaining_percent": snapshot.d7_remaining_percent,
        "d7_resets_at": snapshot.d7_resets_at,
        "status": snapshot.status,
        "generated_at": snapshot.generated_at,
        "today_tokens": snapshot.today_tokens,
        "last_7d_tokens": snapshot.last_7d_tokens,
    }


def usage_snapshot_from_cache(data: dict[str, object]) -> UsageSnapshot | None:
    source = data.get("source")
    status = data.get("status")
    generated_at = _cache_int(data.get("generated_at"))
    if not isinstance(source, str) or not isinstance(status, str) or generated_at is None:
        return None

    return UsageSnapshot(
        source=source,
        h5_remaining_percent=_cache_int(data.get("h5_remaining_percent")),
        h5_resets_at=_cache_int(data.get("h5_resets_at")),
        d7_remaining_percent=_cache_int(data.get("d7_remaining_percent")),
        d7_resets_at=_cache_int(data.get("d7_resets_at")),
        status=status,
        generated_at=generated_at,
        today_tokens=_cache_int(data.get("today_tokens")),
        last_7d_tokens=_cache_int(data.get("last_7d_tokens")),
    )


def _cache_int(value: object) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    return None


def build_stale_usage_payload(payload: Payload, now: int | None = None) -> Payload:
    data = dict(payload.data)
    data["st"] = "stale"
    data["t"] = int(time.time() if now is None else now)
    return Payload("usage", data)


async def run_daemon(args: argparse.Namespace) -> None:
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

    provider = CodexUsageProvider(
        codex_bin=args.codex_bin,
        timeout_sec=args.timeout,
        refresh_token=args.refresh_token,
    )
    registry = DeviceRegistry.load()

    def bind_identity(config: DeviceConfig, identity: BleIdentity) -> DeviceConfig:
        if config.device_id == identity.device_id and config.name == identity.name:
            return config
        updated = DeviceConfig(
            device_id=identity.device_id,
            short_id=identity.short_id,
            alias=config.alias,
            enabled=config.enabled,
            name=identity.name,
            macos_uuid=config.macos_uuid,
            legacy=False,
        )
        registry.upsert(updated)
        try:
            registry.save()
        except OSError:
            logging.exception(
                "Failed to persist identity binding for CodexMeter %s", updated.label
            )
        else:
            logging.info(
                "Bound CodexMeter %s to identity %s",
                updated.label,
                identity.device_id,
            )
        return updated

    manager = DeviceManager(
        registry.enabled_devices(),
        scan_timeout_sec=args.scan_timeout,
        scan_interval_sec=args.scan_interval,
        connect_timeout_sec=args.ble_connect_timeout,
        disconnect_timeout_sec=args.ble_disconnect_timeout,
        write_timeout_sec=args.ble_write_timeout,
        ack_timeout_sec=args.ble_ack_timeout,
        notify_timeout_sec=args.ble_notify_timeout,
        healthcheck_interval_sec=args.ble_healthcheck_interval,
        fallback_device_name=args.device_name,
        fallback_when_empty=not registry.devices,
        identity_observer=bind_identity,
    )
    await manager.broadcast(build_activity_payload(0))

    async def sink(payload: Payload) -> None:
        await manager.broadcast(payload)

    def status_provider() -> dict[str, object]:
        return {"ble": manager.status()}

    event_server = EventServer(sink, status_provider=status_provider)

    await supervise(
        stop_event,
        {
            "events": event_server.run(stop_event),
            "quota": quota_loop(provider, sink, stop_event, args.poll_interval),
            "screen_policy": screen_policy_loop(
                sink,
                None,
                stop_event,
                timeout_sec=args.auto_screen_timeout,
                poll_interval_sec=args.lock_poll_interval,
                connect_control_setter=manager.set_connect_control,
            ),
            "ble": manager.run(stop_event),
        },
    )


def configure_logging(level: str, foreground: bool) -> None:
    APP_DIR.mkdir(parents=True, exist_ok=True)
    handlers: list[logging.Handler] = [build_file_handler()]
    if foreground:
        handlers.append(logging.StreamHandler())
    logging.basicConfig(
        level=getattr(logging, level.upper(), logging.INFO),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        handlers=handlers,
    )


def build_file_handler(
    path: Path = LOG_FILE,
    total_bytes: int = LOG_MAX_TOTAL_BYTES,
) -> RotatingFileHandler:
    return RotatingFileHandler(
        path,
        maxBytes=total_bytes // 2,
        backupCount=1,
        encoding="utf-8",
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the CodexMeter host daemon.")
    parser.add_argument("--codex-bin", default="codex")
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--poll-interval", type=int, default=POLL_INTERVAL_SEC)
    parser.add_argument("--scan-timeout", type=float, default=8.0)
    parser.add_argument("--scan-interval", type=float, default=8.0)
    parser.add_argument(
        "--ble-connect-timeout", type=float, default=BLE_CONNECT_TIMEOUT_SEC
    )
    parser.add_argument(
        "--ble-disconnect-timeout", type=float, default=BLE_DISCONNECT_TIMEOUT_SEC
    )
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
        with DaemonLock():
            asyncio.run(run_daemon(args))
    except DaemonAlreadyRunning as exc:
        logging.error("%s", exc)
        return 1
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
