"""BLE transport for the CodexMeter display."""

from __future__ import annotations

import asyncio
import logging
import sys
from dataclasses import dataclass
from typing import Any

from .payloads import Payload
from .settings import (
    BLE_ACK_TIMEOUT_SEC,
    BLE_WRITE_TIMEOUT_SEC,
    DEVICE_NAME,
    REQ_CHAR_UUID,
    RX_CHAR_UUID,
    SCAN_TIMEOUT_SEC,
    SERVICE_UUID,
    TX_CHAR_UUID,
)

log = logging.getLogger(__name__)


class BleWriteTimeout(RuntimeError):
    """Raised when CoreBluetooth accepts a connection but a GATT write stalls."""


class BleAckTimeout(RuntimeError):
    """Raised when the device does not confirm a payload after it is written."""


class BleAckTracker:
    def __init__(self) -> None:
        self.count = 0
        self.last_text = ""
        self.event = asyncio.Event()

    def notify(self, data: bytearray) -> str:
        text = bytes(data).decode("utf-8", errors="replace")
        self.last_text = text
        self.count += 1
        self.event.set()
        return text

    async def wait_for_next(self, previous_count: int, timeout_sec: float) -> str:
        while self.count <= previous_count:
            self.event.clear()
            if self.count > previous_count:
                break
            try:
                await asyncio.wait_for(self.event.wait(), timeout=timeout_sec)
            except asyncio.TimeoutError as exc:
                raise BleAckTimeout(
                    f"BLE ACK timed out after {timeout_sec:.1f}s"
                ) from exc
        return self.last_text


@dataclass
class BleState:
    last_usage: Payload | None = None
    last_activity: Payload | None = None
    last_payload: Payload | None = None
    connect_control: Payload | None = None


class BleTransport:
    def __init__(
        self,
        device_name: str = DEVICE_NAME,
        scan_timeout_sec: float = SCAN_TIMEOUT_SEC,
        write_timeout_sec: float = BLE_WRITE_TIMEOUT_SEC,
        ack_timeout_sec: float = BLE_ACK_TIMEOUT_SEC,
    ) -> None:
        self.device_name = device_name
        self.scan_timeout_sec = scan_timeout_sec
        self.write_timeout_sec = write_timeout_sec
        self.ack_timeout_sec = ack_timeout_sec

    async def run(
        self,
        queue: "asyncio.Queue[Payload]",
        state: BleState,
        stop_event: asyncio.Event,
    ) -> None:
        backoff = 1.0
        while not stop_event.is_set():
            try:
                target = await self._discover()
                if target is None:
                    log.info(
                        "BLE device %s not found; retrying in %.0fs",
                        self.device_name,
                        backoff,
                    )
                    await _sleep_or_stop(stop_event, backoff)
                    backoff = min(backoff * 2, 60.0)
                    continue

                used = await self._connect_and_write(target, queue, state, stop_event)
                backoff = 1.0 if used else min(backoff * 2, 60.0)
            except asyncio.CancelledError:
                raise
            except (BleWriteTimeout, BleAckTimeout) as exc:
                log.warning("%s; reconnecting in %.0fs", exc, backoff)
                await _sleep_or_stop(stop_event, backoff)
                backoff = min(backoff * 2, 60.0)
            except Exception:
                log.exception("BLE loop error")
                await _sleep_or_stop(stop_event, backoff)
                backoff = min(backoff * 2, 60.0)

    async def _discover(self) -> Any | None:
        try:
            from bleak import BleakScanner
        except ImportError as exc:
            raise RuntimeError("bleak is required for BLE support") from exc

        if sys.platform == "darwin":
            connected = await self._retrieve_connected_macos()
            if connected is not None:
                return connected

        log.info("Scanning for BLE device %s", self.device_name)
        return await BleakScanner.find_device_by_name(
            self.device_name, timeout=self.scan_timeout_sec
        )

    async def _retrieve_connected_macos(self) -> Any | None:
        """Return a CoreBluetooth-connected peripheral when scans cannot see it."""

        try:
            from CoreBluetooth import CBUUID  # type: ignore
            from bleak.backends.corebluetooth.CentralManagerDelegate import (  # type: ignore
                CentralManagerDelegate,
            )
            from bleak.backends.device import BLEDevice  # type: ignore
        except Exception:
            return None

        try:
            manager = CentralManagerDelegate()
            await manager.wait_until_ready()
            central = manager.central_manager
            for service in (SERVICE_UUID, "1812"):
                peripherals = central.retrieveConnectedPeripheralsWithServices_(
                    [CBUUID.UUIDWithString_(service)]
                )
                for peripheral in peripherals or []:
                    name = peripheral.name()
                    if service == SERVICE_UUID or name == self.device_name:
                        address = peripheral.identifier().UUIDString()
                        log.info(
                            "Found connected BLE peripheral %s [%s]",
                            name,
                            address,
                        )
                        return BLEDevice(address, name, (peripheral, manager))
        except Exception:
            log.debug("macOS connected-peripheral lookup failed", exc_info=True)
        return None

    async def _connect_and_write(
        self,
        target: Any,
        queue: "asyncio.Queue[Payload]",
        state: BleState,
        stop_event: asyncio.Event,
    ) -> bool:
        from bleak import BleakClient
        from bleak.exc import BleakError

        refresh_requested = asyncio.Event()

        def on_refresh(_char: Any, _data: bytearray) -> None:
            log.info("Device requested a refresh")
            refresh_requested.set()

        display = getattr(target, "address", str(target))
        log.info("Connecting to %s", display)
        used_successfully = False
        async with BleakClient(target) as client:
            log.info("Connected to %s", display)
            with contextlib_suppress_bleak():
                await client.start_notify(REQ_CHAR_UUID, on_refresh)
            ack_tracker: BleAckTracker | None = BleAckTracker()
            try:
                await client.start_notify(
                    TX_CHAR_UUID,
                    lambda char, data: self._on_ack(char, data, ack_tracker),
                )
            except (BleakError, ValueError) as exc:
                log.debug("BLE ACK notifications unavailable: %s", exc)
                ack_tracker = None

            if state.connect_control is not None:
                await self._write_payload(client, state.connect_control, ack_tracker)
                used_successfully = True
            if state.last_usage is not None:
                await self._write_payload(client, state.last_usage, ack_tracker)
                used_successfully = True
            if state.last_activity is not None:
                await self._write_payload(client, state.last_activity, ack_tracker)
                used_successfully = True

            while client.is_connected and not stop_event.is_set():
                payload_task = asyncio.create_task(queue.get())
                refresh_task = asyncio.create_task(refresh_requested.wait())
                stop_task = asyncio.create_task(stop_event.wait())
                done, pending = await asyncio.wait(
                    {payload_task, refresh_task, stop_task},
                    return_when=asyncio.FIRST_COMPLETED,
                    timeout=30,
                )
                for task in pending:
                    task.cancel()

                if stop_task in done:
                    break
                if refresh_task in done and refresh_requested.is_set():
                    refresh_requested.clear()
                    if state.last_usage is not None:
                        await self._write_payload(client, state.last_usage, ack_tracker)
                        used_successfully = True
                    continue
                if payload_task in done:
                    payload = payload_task.result()
                    if not self._should_write_payload(payload, state):
                        log.info("Skipped stale BLE payload %s", payload.kind)
                        continue
                    await self._write_payload(client, payload, ack_tracker)
                    state.last_payload = payload
                    if payload.kind == "usage":
                        state.last_usage = payload
                    elif payload.kind == "activity":
                        state.last_activity = payload
                    used_successfully = True

        log.info("BLE disconnected from %s", display)
        return used_successfully

    async def _write_payload(
        self,
        client: Any,
        payload: Payload,
        ack_tracker: BleAckTracker | None = None,
    ) -> None:
        data = payload.to_json_bytes()
        log.debug(
            "BLE write %s: %s",
            payload.kind,
            data.decode("utf-8", errors="replace"),
        )
        previous_ack_count = ack_tracker.count if ack_tracker is not None else 0
        try:
            await asyncio.wait_for(
                client.write_gatt_char(RX_CHAR_UUID, data, response=True),
                timeout=self.write_timeout_sec,
            )
        except asyncio.TimeoutError as exc:
            raise BleWriteTimeout(
                f"BLE write {payload.kind} timed out after {self.write_timeout_sec:.1f}s"
            ) from exc

        if ack_tracker is None:
            return

        try:
            await ack_tracker.wait_for_next(previous_ack_count, self.ack_timeout_sec)
        except BleAckTimeout as exc:
            raise BleAckTimeout(
                f"BLE ACK for {payload.kind} timed out after {self.ack_timeout_sec:.1f}s"
            ) from exc

    def _should_write_payload(self, payload: Payload, state: BleState) -> bool:
        if payload.kind != "control":
            return True
        if payload.data.get("cmd") != "screen":
            return True
        if payload.data.get("on") is True and state.connect_control is None:
            return False
        return True

    def _on_ack(
        self,
        _char: Any,
        data: bytearray,
        ack_tracker: BleAckTracker | None = None,
    ) -> None:
        if ack_tracker is not None:
            text = ack_tracker.notify(data)
        else:
            text = bytes(data).decode("utf-8", errors="replace")
        log.debug("BLE ack: %s", text)


async def _sleep_or_stop(stop_event: asyncio.Event, seconds: float) -> None:
    try:
        await asyncio.wait_for(stop_event.wait(), timeout=seconds)
    except asyncio.TimeoutError:
        pass


class contextlib_suppress_bleak:
    def __enter__(self) -> None:
        return None

    def __exit__(self, exc_type: Any, exc: BaseException | None, _tb: Any) -> bool:
        if exc is None:
            return False
        try:
            from bleak.exc import BleakError
        except Exception:
            BleakError = Exception  # type: ignore
        if isinstance(exc, (BleakError, ValueError)):
            log.debug("Optional BLE operation failed: %s", exc)
            return True
        return False
