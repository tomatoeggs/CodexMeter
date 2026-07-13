"""BLE transport for the CodexMeter display."""

from __future__ import annotations

import asyncio
import json
import logging
import re
import sys
from dataclasses import dataclass
from typing import Any, Callable

from .payloads import Payload
from .settings import (
    BLE_ACK_TIMEOUT_SEC,
    BLE_CONNECT_TIMEOUT_SEC,
    BLE_DISCONNECT_TIMEOUT_SEC,
    BLE_HEALTHCHECK_INTERVAL_SEC,
    BLE_NOTIFY_TIMEOUT_SEC,
    BLE_WRITE_TIMEOUT_SEC,
    DEVICE_NAME,
    IDENTITY_CHAR_UUID,
    REQ_CHAR_UUID,
    RX_CHAR_UUID,
    SCAN_TIMEOUT_SEC,
    SERVICE_UUID,
    TX_CHAR_UUID,
)

log = logging.getLogger(__name__)


class MacConnectedDeviceLookup:
    """Reusable CoreBluetooth lookup for peripherals that are not advertising."""

    def __init__(
        self,
        service_uuids: tuple[str, ...] = (SERVICE_UUID, "1812"),
        ready_timeout_sec: float = 5.0,
    ) -> None:
        self.service_uuids = service_uuids
        self.ready_timeout_sec = ready_timeout_sec
        self._manager: Any | None = None
        self.last_error: str | None = None

    async def __call__(self) -> list[Any]:
        if sys.platform != "darwin":
            return []
        try:
            from CoreBluetooth import CBUUID  # type: ignore
            from bleak.backends.corebluetooth.CentralManagerDelegate import (  # type: ignore
                CentralManagerDelegate,
            )
            from bleak.backends.device import BLEDevice  # type: ignore
        except Exception:
            self.last_error = "CoreBluetooth connected lookup unavailable"
            return []

        try:
            if self._manager is None:
                self._manager = CentralManagerDelegate()
                await asyncio.wait_for(
                    self._manager.wait_until_ready(), timeout=self.ready_timeout_sec
                )
            central = self._manager.central_manager
            result: list[Any] = []
            seen: set[str] = set()
            for service in self.service_uuids:
                peripherals = central.retrieveConnectedPeripheralsWithServices_(
                    [CBUUID.UUIDWithString_(service)]
                )
                for peripheral in peripherals or []:
                    address = str(peripheral.identifier().UUIDString())
                    if address in seen:
                        continue
                    seen.add(address)
                    name = peripheral.name()
                    result.append(
                        BLEDevice(
                            address,
                            str(name) if name is not None else None,
                            (peripheral, self._manager),
                        )
                    )
            self.last_error = None
            return result
        except Exception as exc:
            self._manager = None
            self.last_error = f"{type(exc).__name__}: {exc}"
            log.debug("macOS connected-peripheral lookup failed", exc_info=True)
            return []


async def retrieve_connected_devices_macos(
    service_uuids: tuple[str, ...] = (SERVICE_UUID, "1812"),
) -> list[Any]:
    return await MacConnectedDeviceLookup(service_uuids)()


class BleWriteTimeout(RuntimeError):
    """Raised when CoreBluetooth accepts a connection but a GATT write stalls."""


class BleConnectTimeout(RuntimeError):
    """Raised when CoreBluetooth does not finish connecting in time."""


class BleAckTimeout(RuntimeError):
    """Raised when the device does not confirm a payload after it is written."""


class BleAckError(RuntimeError):
    """Raised when the device reports that a payload was not accepted."""


class BleNotifyError(RuntimeError):
    """Raised when mandatory BLE notifications cannot be subscribed."""


class BleIdentityError(RuntimeError):
    """Raised when a connected peripheral has an invalid or unexpected identity."""


@dataclass(frozen=True)
class BleIdentity:
    device_id: str
    short_id: str
    name: str

    @classmethod
    def from_bytes(cls, raw: bytes | bytearray) -> "BleIdentity":
        try:
            data = json.loads(bytes(raw).decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise BleIdentityError("BLE identity is not valid JSON") from exc
        if not isinstance(data, dict):
            raise BleIdentityError("BLE identity must be a JSON object")
        device_id = data.get("device_id")
        short_id = data.get("short_id")
        name = data.get("name")
        if not all(isinstance(value, str) and value for value in (device_id, short_id, name)):
            raise BleIdentityError("BLE identity is missing required fields")
        normalized_device_id = str(device_id).lower()
        normalized_short_id = str(short_id).upper()
        normalized_name = str(name)
        if not re.fullmatch(r"codexmeter-[0-9a-f]{12}", normalized_device_id):
            raise BleIdentityError("BLE identity device id has an invalid format")
        if not re.fullmatch(r"[0-9A-F]{6}", normalized_short_id):
            raise BleIdentityError("BLE identity short id has an invalid format")
        device_short_id = normalized_device_id.removeprefix("codexmeter-")[:6].upper()
        if device_short_id != normalized_short_id:
            raise BleIdentityError("BLE identity device id does not match its short id")
        if normalized_name != f"{DEVICE_NAME}-{normalized_short_id}":
            raise BleIdentityError("BLE identity name does not match its short id")
        return cls(normalized_device_id, normalized_short_id, normalized_name)


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
    desired_control: Payload | None = None
    pending_alert: Payload | None = None
    observed_device_id: str | None = None
    connected: bool = False
    last_discovered_at: float | None = None
    last_discovery_source: str | None = None
    last_discovered_address: str | None = None
    last_connected_at: float | None = None
    last_disconnected_at: float | None = None
    last_write_at: float | None = None
    last_ack_at: float | None = None
    consecutive_failures: int = 0
    last_error: str | None = None

    def status(self, queue_depth: int = 0, now: float | None = None) -> dict[str, Any]:
        now = asyncio.get_running_loop().time() if now is None else now

        def age(value: float | None) -> float | None:
            if value is None:
                return None
            return round(max(0.0, now - value), 1)

        return {
            "connected": self.connected,
            "queue_depth": queue_depth,
            "last_discovered_age_sec": age(self.last_discovered_at),
            "last_discovery_source": self.last_discovery_source,
            "last_discovered_address": self.last_discovered_address,
            "last_connected_age_sec": age(self.last_connected_at),
            "last_disconnected_age_sec": age(self.last_disconnected_at),
            "last_write_age_sec": age(self.last_write_at),
            "last_ack_age_sec": age(self.last_ack_at),
            "consecutive_failures": self.consecutive_failures,
            "last_error": self.last_error,
            "last_payload": self.last_payload.kind if self.last_payload else None,
            "pending_alert": self.pending_alert.data.get("id") if self.pending_alert else None,
            "observed_device_id": self.observed_device_id,
        }


class BleTransport:
    def __init__(
        self,
        device_name: str = DEVICE_NAME,
        scan_timeout_sec: float = SCAN_TIMEOUT_SEC,
        connect_timeout_sec: float = BLE_CONNECT_TIMEOUT_SEC,
        disconnect_timeout_sec: float = BLE_DISCONNECT_TIMEOUT_SEC,
        write_timeout_sec: float = BLE_WRITE_TIMEOUT_SEC,
        ack_timeout_sec: float = BLE_ACK_TIMEOUT_SEC,
        notify_timeout_sec: float = BLE_NOTIFY_TIMEOUT_SEC,
        healthcheck_interval_sec: float = BLE_HEALTHCHECK_INTERVAL_SEC,
    ) -> None:
        self.device_name = device_name
        self.scan_timeout_sec = scan_timeout_sec
        self.connect_timeout_sec = connect_timeout_sec
        self.disconnect_timeout_sec = disconnect_timeout_sec
        self.write_timeout_sec = write_timeout_sec
        self.ack_timeout_sec = ack_timeout_sec
        self.notify_timeout_sec = notify_timeout_sec
        self.healthcheck_interval_sec = healthcheck_interval_sec
        self._connected_lookup = MacConnectedDeviceLookup()

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
            except (
                BleConnectTimeout,
                BleWriteTimeout,
                BleAckTimeout,
                BleAckError,
                BleNotifyError,
            ) as exc:
                state.connected = False
                state.consecutive_failures += 1
                state.last_error = str(exc)
                log.warning("%s; reconnecting in %.0fs", exc, backoff)
                await _sleep_or_stop(stop_event, backoff)
                backoff = min(backoff * 2, 60.0)
            except Exception:
                state.connected = False
                state.consecutive_failures += 1
                state.last_error = "BLE loop error"
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

        for device in await self._connected_lookup():
            name = getattr(device, "name", None)
            if not self._connected_name_matches(name):
                log.debug("Skipping connected BLE peripheral %s", name or "<unnamed>")
                continue
            log.info(
                "Found connected BLE peripheral %s [%s]",
                name,
                getattr(device, "address", "<unknown>"),
            )
            return device
        return None

    def _connected_name_matches(self, name: Any) -> bool:
        return name is not None and str(name) == self.device_name

    def _create_client(self, target: Any) -> Any:
        from bleak import BleakClient

        return BleakClient(target, timeout=self.connect_timeout_sec)

    async def _disconnect(self, client: Any, display: str) -> None:
        try:
            await asyncio.wait_for(
                client.disconnect(), timeout=self.disconnect_timeout_sec
            )
        except asyncio.TimeoutError:
            log.warning(
                "BLE disconnect from %s timed out after %.1fs",
                display,
                self.disconnect_timeout_sec,
            )
        except Exception:
            log.warning("BLE disconnect from %s failed", display, exc_info=True)

    async def _connect_and_write(
        self,
        target: Any,
        queue: "asyncio.Queue[Payload]",
        state: BleState,
        stop_event: asyncio.Event,
        *,
        require_identity: bool = False,
        identity_validator: Callable[[BleIdentity], None] | None = None,
    ) -> bool:
        from bleak.exc import BleakError

        refresh_requested = asyncio.Event()

        def on_refresh(_char: Any, _data: bytearray) -> None:
            log.info("Device requested a refresh")
            refresh_requested.set()

        display = getattr(target, "address", str(target))
        log.info("Connecting to %s", display)
        used_successfully = False
        client = self._create_client(target)
        loop = asyncio.get_running_loop()
        try:
            try:
                await asyncio.wait_for(
                    client.connect(), timeout=self.connect_timeout_sec
                )
            except asyncio.TimeoutError as exc:
                raise BleConnectTimeout(
                    f"BLE connect timed out after {self.connect_timeout_sec:.1f}s"
                ) from exc
            log.info("Connected to %s", display)
            if require_identity:
                identity = await self._read_identity(client)
                if identity_validator is not None:
                    identity_validator(identity)
                state.observed_device_id = identity.device_id
            ack_tracker: BleAckTracker | None = BleAckTracker()
            try:
                await self._start_notify(
                    client,
                    TX_CHAR_UUID,
                    lambda char, data: self._on_ack(char, data, ack_tracker),
                )
            except (BleakError, ValueError, BleNotifyError) as exc:
                raise BleNotifyError("BLE ACK notifications unavailable") from exc

            try:
                await self._start_notify(client, REQ_CHAR_UUID, on_refresh)
            except (BleakError, ValueError, BleNotifyError) as exc:
                log.debug("BLE refresh notifications unavailable: %s", exc)

            state.connected = True
            state.last_connected_at = loop.time()
            state.last_error = None

            try:
                reconnect_control = state.desired_control or state.connect_control
                if reconnect_control is not None and self._should_write_payload(
                    reconnect_control, state
                ):
                    await self._write_payload(
                        client, reconnect_control, ack_tracker, state
                    )
                    used_successfully = True
                if state.last_usage is not None:
                    await self._write_payload(
                        client, state.last_usage, ack_tracker, state
                    )
                    used_successfully = True
                if state.last_activity is not None:
                    await self._write_payload(
                        client, state.last_activity, ack_tracker, state
                    )
                    used_successfully = True
                if state.pending_alert is not None:
                    pending_alert = state.pending_alert
                    await self._write_reliably(
                        client, pending_alert, ack_tracker, state
                    )
                    used_successfully = True

                while client.is_connected and not stop_event.is_set():
                    payload_task = asyncio.create_task(queue.get())
                    refresh_task = asyncio.create_task(refresh_requested.wait())
                    stop_task = asyncio.create_task(stop_event.wait())
                    health_task = asyncio.create_task(
                        asyncio.sleep(self.healthcheck_interval_sec)
                    )
                    done, pending = await asyncio.wait(
                        {payload_task, refresh_task, stop_task, health_task},
                        return_when=asyncio.FIRST_COMPLETED,
                    )
                    for task in pending:
                        task.cancel()
                    await asyncio.gather(*pending, return_exceptions=True)

                    payload = payload_task.result() if payload_task in done else None
                    if payload is not None and payload.kind == "alert":
                        state.pending_alert = payload

                    if stop_task in done:
                        break
                    if not client.is_connected:
                        break
                    if payload is not None:
                        if not self._should_write_payload(payload, state):
                            log.info("Skipped stale BLE payload %s", payload.kind)
                            if state.pending_alert is payload:
                                state.pending_alert = None
                            continue
                        await self._write_reliably(client, payload, ack_tracker, state)
                        used_successfully = True
                        continue
                    if refresh_task in done and refresh_requested.is_set():
                        refresh_requested.clear()
                        if state.last_usage is not None:
                            await self._write_payload(
                                client, state.last_usage, ack_tracker, state
                            )
                            used_successfully = True
                        continue
                    if health_task in done:
                        heartbeat = state.last_usage or state.last_activity
                        if heartbeat is not None:
                            log.info("BLE health check with %s payload", heartbeat.kind)
                            await self._write_payload(client, heartbeat, ack_tracker, state)
                            used_successfully = True
            finally:
                state.connected = False
                state.last_disconnected_at = loop.time()
        finally:
            await self._disconnect(client, display)
            log.info("BLE disconnected from %s", display)
        return used_successfully

    async def connect_and_write(
        self,
        target: Any,
        queue: "asyncio.Queue[Payload]",
        state: BleState,
        stop_event: asyncio.Event,
        *,
        require_identity: bool = False,
        identity_validator: Callable[[BleIdentity], None] | None = None,
    ) -> bool:
        return await self._connect_and_write(
            target,
            queue,
            state,
            stop_event,
            require_identity=require_identity,
            identity_validator=identity_validator,
        )

    async def _read_identity(self, client: Any) -> BleIdentity:
        try:
            raw = await asyncio.wait_for(
                client.read_gatt_char(IDENTITY_CHAR_UUID),
                timeout=self.notify_timeout_sec,
            )
        except asyncio.TimeoutError as exc:
            raise BleIdentityError("BLE identity read timed out") from exc
        except Exception as exc:
            raise BleIdentityError("BLE identity characteristic unavailable") from exc
        return BleIdentity.from_bytes(raw)

    async def _write_payload(
        self,
        client: Any,
        payload: Payload,
        ack_tracker: BleAckTracker | None = None,
        state: BleState | None = None,
    ) -> None:
        data = payload.to_json_bytes()
        log.debug(
            "BLE write %s: %s",
            payload.kind,
            data.decode("utf-8", errors="replace"),
        )
        previous_ack_count = ack_tracker.count if ack_tracker is not None else 0
        loop = asyncio.get_running_loop()
        if state is not None:
            state.last_write_at = loop.time()
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
            raise BleNotifyError("BLE ACK tracker is unavailable")

        try:
            ack_text = await ack_tracker.wait_for_next(
                previous_ack_count, self.ack_timeout_sec
            )
        except BleAckTimeout as exc:
            raise BleAckTimeout(
                f"BLE ACK for {payload.kind} timed out after {self.ack_timeout_sec:.1f}s"
            ) from exc
        self._require_ack(payload, ack_text)
        if state is not None:
            self._remember_payload(payload, state)
            state.last_ack_at = loop.time()
            state.consecutive_failures = 0
            state.last_error = None

    async def _write_reliably(
        self,
        client: Any,
        payload: Payload,
        ack_tracker: BleAckTracker,
        state: BleState,
    ) -> None:
        if payload.kind == "alert":
            state.pending_alert = payload
        await self._write_payload(client, payload, ack_tracker, state)
        if state.pending_alert is payload:
            state.pending_alert = None

    async def _start_notify(self, client: Any, uuid: str, callback: Any) -> None:
        try:
            await asyncio.wait_for(
                client.start_notify(uuid, callback),
                timeout=self.notify_timeout_sec,
            )
        except asyncio.TimeoutError as exc:
            raise BleNotifyError(
                f"BLE notify {uuid} timed out after {self.notify_timeout_sec:.1f}s"
            ) from exc

    @staticmethod
    def _remember_payload(payload: Payload, state: BleState) -> None:
        state.last_payload = payload
        if payload.kind == "usage":
            state.last_usage = payload
        elif payload.kind == "activity":
            state.last_activity = payload
        elif payload.kind == "control":
            state.desired_control = payload

    @staticmethod
    def remember_desired(payload: Payload, state: BleState) -> None:
        if payload.kind == "usage":
            state.last_usage = payload
        elif payload.kind == "activity":
            state.last_activity = payload
        elif payload.kind == "control":
            state.desired_control = payload

    @staticmethod
    def _require_ack(payload: Payload, text: str) -> None:
        try:
            data = json.loads(text)
        except json.JSONDecodeError as exc:
            raise BleAckError(f"BLE ACK for {payload.kind} was invalid: {text!r}") from exc
        if not isinstance(data, dict) or data.get("ack") is not True:
            raise BleAckError(f"BLE NACK for {payload.kind}: {text}")

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
