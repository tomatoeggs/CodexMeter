"""Multi-device BLE coordination for CodexMeter displays."""

from __future__ import annotations

import asyncio
import logging
import random
import time
from dataclasses import dataclass, field
from typing import Any, Callable

from .ble import (
    BleAckError,
    BleAckTimeout,
    BleIdentity,
    BleIdentityError,
    BleNotifyError,
    BleState,
    BleTransport,
    BleWriteTimeout,
)
from .device_registry import (
    DeviceConfig,
    default_legacy_device,
    is_full_device_id,
    normalize_device_id,
    normalize_short_id,
    parse_short_id_from_name,
)
from .payloads import Payload
from .queueing import put_latest
from .settings import DEVICE_NAME, SERVICE_UUID
from .supervision import supervise

log = logging.getLogger(__name__)
IdentityObserver = Callable[[DeviceConfig, BleIdentity], DeviceConfig]


@dataclass
class DiscoveredDevice:
    address: str
    name: str | None
    short_id: str | None
    device: Any
    seen_at: float = field(default_factory=time.monotonic)

    def status(self, now: float | None = None) -> dict[str, object]:
        now = time.monotonic() if now is None else now
        return {
            "address": self.address,
            "name": self.name,
            "short_id": self.short_id,
            "last_seen_age_sec": round(max(0.0, now - self.seen_at), 1),
        }


class DiscoveryService:
    def __init__(
        self,
        *,
        scan_timeout_sec: float = 4.0,
        scan_interval_sec: float = 8.0,
        max_age_sec: float = 30.0,
    ) -> None:
        self.scan_timeout_sec = scan_timeout_sec
        self.scan_interval_sec = scan_interval_sec
        self.max_age_sec = max_age_sec
        self._devices: dict[str, DiscoveredDevice] = {}
        self._changed = asyncio.Event()
        self._last_error: str | None = None

    async def run(self, stop_event: asyncio.Event) -> None:
        while not stop_event.is_set():
            try:
                await self.scan_once()
            except asyncio.CancelledError:
                raise
            except Exception as exc:
                self._last_error = str(exc)
                log.warning("BLE discovery failed: %s", exc)
            await _sleep_or_stop(stop_event, self.scan_interval_sec)

    async def scan_once(self) -> list[DiscoveredDevice]:
        found = await scan_codexmeter_devices(timeout=self.scan_timeout_sec)
        now = time.monotonic()
        for device in found:
            device.seen_at = now
            self._devices[device.address] = device
        self._prune(now)
        self._last_error = None
        self._changed.set()
        log.debug("Discovered %d CodexMeter BLE device(s)", len(found))
        return found

    async def wait_for(
        self,
        config: DeviceConfig,
        stop_event: asyncio.Event,
    ) -> DiscoveredDevice | None:
        while not stop_event.is_set():
            match = self.find(config)
            if match is not None:
                return match
            self._changed.clear()
            try:
                await asyncio.wait_for(
                    _wait_any(self._changed.wait(), stop_event.wait()), timeout=10.0
                )
            except asyncio.TimeoutError:
                pass
        return None

    def find(self, config: DeviceConfig) -> DiscoveredDevice | None:
        now = time.monotonic()
        self._prune(now)
        for device in self._devices.values():
            if config.matches(
                short_id=device.short_id,
                name=device.name,
                address=device.address,
            ):
                return device
        return None

    def unknown_devices(self, configs: list[DeviceConfig]) -> list[dict[str, object]]:
        now = time.monotonic()
        self._prune(now)
        result: list[dict[str, object]] = []
        for device in self._devices.values():
            known = any(
                config.matches(
                    short_id=device.short_id,
                    name=device.name,
                    address=device.address,
                )
                for config in configs
            )
            if not known:
                result.append(device.status(now))
        return result

    def _prune(self, now: float) -> None:
        stale = [
            address
            for address, device in self._devices.items()
            if now - device.seen_at > self.max_age_sec
        ]
        for address in stale:
            del self._devices[address]


@dataclass
class DeviceSlot:
    config: DeviceConfig
    queue: "asyncio.Queue[Payload]" = field(
        default_factory=lambda: asyncio.Queue(maxsize=32)
    )
    state: BleState = field(default_factory=BleState)

    def status(self, now: float | None = None) -> dict[str, object]:
        data = self.state.status(queue_depth=self.queue.qsize(), now=now)
        data.update(
            {
                "device_id": self.config.device_id,
                "short_id": self.config.short_id,
                "alias": self.config.alias,
                "label": self.config.label,
                "enabled": self.config.enabled,
            }
        )
        return data


class DeviceWorker:
    def __init__(
        self,
        slot: DeviceSlot,
        discovery: DiscoveryService,
        transport: BleTransport,
        identity_observer: IdentityObserver | None = None,
    ) -> None:
        self.slot = slot
        self.discovery = discovery
        self.transport = transport
        self.identity_observer = identity_observer

    async def run(self, stop_event: asyncio.Event) -> None:
        backoff = 1.0
        label = self.slot.config.label
        while not stop_event.is_set():
            try:
                discovered = await self.discovery.wait_for(self.slot.config, stop_event)
                if discovered is None:
                    break
                log.info(
                    "Connecting CodexMeter %s at %s",
                    label,
                    discovered.address,
                )
                used = await self.transport.connect_and_write(
                    discovered.device,
                    self.slot.queue,
                    self.slot.state,
                    stop_event,
                    require_identity=not self.slot.config.legacy,
                    identity_validator=self._validate_identity,
                )
                backoff = 1.0 if used else min(backoff * 2, 60.0)
            except asyncio.CancelledError:
                raise
            except (
                BleWriteTimeout,
                BleAckTimeout,
                BleAckError,
                BleNotifyError,
                BleIdentityError,
            ) as exc:
                self._record_failure(str(exc))
                await _sleep_or_stop(stop_event, _with_jitter(backoff))
                backoff = min(backoff * 2, 60.0)
            except Exception as exc:
                self._record_failure("BLE worker error")
                log.exception("BLE worker error for %s: %s", label, exc)
                await _sleep_or_stop(stop_event, _with_jitter(backoff))
                backoff = min(backoff * 2, 60.0)

    def _record_failure(self, error: str) -> None:
        state = self.slot.state
        state.connected = False
        state.consecutive_failures += 1
        state.last_error = error
        log.warning("CodexMeter %s: %s", self.slot.config.label, error)

    def _validate_identity(self, identity: BleIdentity) -> None:
        config = self.slot.config
        if config.short_id and normalize_short_id(identity.short_id) != config.short_id:
            raise BleIdentityError(
                f"BLE identity short id {identity.short_id} does not match {config.short_id}"
            )
        if is_full_device_id(config.device_id) and normalize_device_id(
            identity.device_id
        ) != normalize_device_id(config.device_id):
            raise BleIdentityError(
                f"BLE identity {identity.device_id} does not match {config.device_id}"
            )
        expected_name = config.name
        if expected_name and identity.name != expected_name:
            raise BleIdentityError(
                f"BLE identity name {identity.name} does not match {expected_name}"
            )
        if self.identity_observer is not None and not is_full_device_id(config.device_id):
            self.slot.config = self.identity_observer(config, identity)


class DeviceManager:
    def __init__(
        self,
        configs: list[DeviceConfig],
        *,
        scan_timeout_sec: float = 4.0,
        scan_interval_sec: float = 8.0,
        write_timeout_sec: float,
        ack_timeout_sec: float,
        notify_timeout_sec: float,
        healthcheck_interval_sec: float,
        fallback_device_name: str = DEVICE_NAME,
        fallback_when_empty: bool = True,
        identity_observer: IdentityObserver | None = None,
    ) -> None:
        active_configs = [config for config in configs if config.enabled]
        if not active_configs and fallback_when_empty:
            active_configs = [default_legacy_device(fallback_device_name)]
        self.slots = [DeviceSlot(config) for config in active_configs]
        self.discovery = DiscoveryService(
            scan_timeout_sec=scan_timeout_sec,
            scan_interval_sec=scan_interval_sec,
        )
        self._workers = [
            DeviceWorker(
                slot,
                self.discovery,
                BleTransport(
                    device_name=slot.config.name or DEVICE_NAME,
                    scan_timeout_sec=scan_timeout_sec,
                    write_timeout_sec=write_timeout_sec,
                    ack_timeout_sec=ack_timeout_sec,
                    notify_timeout_sec=notify_timeout_sec,
                    healthcheck_interval_sec=healthcheck_interval_sec,
                ),
                identity_observer,
            )
            for slot in self.slots
        ]

    async def run(self, stop_event: asyncio.Event) -> None:
        services = {"ble_discovery": self.discovery.run(stop_event)}
        services.update(
            {
                f"ble_{index}_{worker.slot.config.device_id}": worker.run(stop_event)
                for index, worker in enumerate(self._workers)
            }
        )
        await supervise(stop_event, services)

    async def broadcast(self, payload: Payload) -> None:
        for slot in self.slots:
            BleTransport.remember_desired(payload, slot.state)
            await put_latest(slot.queue, payload)

    def set_connect_control(self, payload: Payload | None) -> None:
        for slot in self.slots:
            slot.state.connect_control = payload

    def status(self) -> dict[str, object]:
        now = asyncio.get_running_loop().time()
        devices = [slot.status(now=now) for slot in self.slots]
        connected_count = sum(1 for item in devices if item.get("connected") is True)
        return {
            "connected": connected_count > 0,
            "configured_count": len(self.slots),
            "connected_count": connected_count,
            "devices": devices,
            "unknown_devices": self.discovery.unknown_devices(
                [slot.config for slot in self.slots]
            ),
        }


async def scan_codexmeter_devices(timeout: float = 4.0) -> list[DiscoveredDevice]:
    try:
        from bleak import BleakScanner
    except ImportError as exc:
        raise RuntimeError("bleak is required for BLE support") from exc

    raw = await BleakScanner.discover(timeout=timeout, return_adv=True)
    result: list[DiscoveredDevice] = []
    for address, item in raw.items():
        device, adv = item
        name = getattr(adv, "local_name", None) or getattr(device, "name", None)
        service_uuids = {
            str(uuid).lower() for uuid in getattr(adv, "service_uuids", []) or []
        }
        is_codexmeter = str(SERVICE_UUID).lower() in service_uuids
        if name == DEVICE_NAME or (isinstance(name, str) and name.startswith(f"{DEVICE_NAME}-")):
            is_codexmeter = True
        if not is_codexmeter:
            continue
        result.append(
            DiscoveredDevice(
                address=str(address),
                name=name,
                short_id=parse_short_id_from_name(name),
                device=device,
            )
        )
    return result


async def _sleep_or_stop(stop_event: asyncio.Event, seconds: float) -> None:
    try:
        await asyncio.wait_for(stop_event.wait(), timeout=seconds)
    except asyncio.TimeoutError:
        pass


async def _wait_any(*awaitables: Any) -> None:
    tasks = [asyncio.create_task(awaitable) for awaitable in awaitables]
    try:
        done, pending = await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
        for task in pending:
            task.cancel()
        await asyncio.gather(*pending, return_exceptions=True)
        for task in done:
            task.result()
    finally:
        for task in tasks:
            if not task.done():
                task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)


def _with_jitter(seconds: float) -> float:
    return seconds * random.uniform(0.8, 1.2)
