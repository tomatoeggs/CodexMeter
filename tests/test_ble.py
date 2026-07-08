import asyncio

from codexmeter.ble import (
    BleAckTimeout,
    BleAckTracker,
    BleNotifyError,
    BleState,
    BleTransport,
    BleWriteTimeout,
)
from codexmeter.payloads import build_activity_payload, build_screen_control_payload


class FakeWriteClient:
    def __init__(self, *, delay_sec: float = 0.0) -> None:
        self.delay_sec = delay_sec
        self.writes: list[tuple[str, bytes, bool]] = []

    async def write_gatt_char(self, uuid: str, data: bytes, *, response: bool) -> None:
        self.writes.append((uuid, data, response))
        if self.delay_sec:
            await asyncio.sleep(self.delay_sec)


class FakeNotifyClient:
    def __init__(self, *, delay_sec: float = 0.0) -> None:
        self.delay_sec = delay_sec
        self.notifications: list[str] = []

    async def start_notify(self, uuid: str, _callback) -> None:
        self.notifications.append(uuid)
        if self.delay_sec:
            await asyncio.sleep(self.delay_sec)


def test_ble_skips_stale_screen_on_when_connect_wake_is_disabled():
    state = BleState(connect_control=None)
    transport = BleTransport()
    payload = build_screen_control_payload(True, "mac_unlocked", now=1)

    assert transport._should_write_payload(payload, state) is False


def test_ble_allows_screen_off_when_connect_wake_is_disabled():
    state = BleState(connect_control=None)
    transport = BleTransport()
    payload = build_screen_control_payload(False, "mac_locked", now=1)

    assert transport._should_write_payload(payload, state) is True


def test_ble_write_payload_times_out_when_gatt_write_stalls():
    async def run() -> None:
        transport = BleTransport(write_timeout_sec=0.01, ack_timeout_sec=0.01)
        client = FakeWriteClient(delay_sec=1.0)
        payload = build_activity_payload(1, now=1)

        try:
            await transport._write_payload(client, payload)
        except BleWriteTimeout:
            return
        raise AssertionError("expected BLE write timeout")

    asyncio.run(run())


def test_ble_write_payload_rejects_missing_ack_tracker():
    async def run() -> None:
        transport = BleTransport(write_timeout_sec=0.1, ack_timeout_sec=0.01)
        client = FakeWriteClient()
        payload = build_activity_payload(1, now=1)

        try:
            await transport._write_payload(client, payload)
        except BleNotifyError:
            return
        raise AssertionError("expected BLE notify error")

    asyncio.run(run())


def test_ble_write_payload_times_out_without_device_ack():
    async def run() -> None:
        transport = BleTransport(write_timeout_sec=0.1, ack_timeout_sec=0.01)
        client = FakeWriteClient()
        payload = build_activity_payload(1, now=1)

        try:
            await transport._write_payload(client, payload, BleAckTracker())
        except BleAckTimeout:
            return
        raise AssertionError("expected BLE ACK timeout")

    asyncio.run(run())


def test_ble_start_notify_times_out():
    async def run() -> None:
        transport = BleTransport(notify_timeout_sec=0.01)
        client = FakeNotifyClient(delay_sec=1.0)

        try:
            await transport._start_notify(client, "uuid", lambda *_: None)
        except BleNotifyError:
            return
        raise AssertionError("expected BLE notify timeout")

    asyncio.run(run())


def test_ble_write_payload_accepts_device_ack():
    async def run() -> None:
        transport = BleTransport(write_timeout_sec=0.1, ack_timeout_sec=0.1)
        client = FakeWriteClient()
        tracker = BleAckTracker()
        payload = build_activity_payload(1, now=1)

        async def notify_ack() -> None:
            await asyncio.sleep(0)
            tracker.notify(bytearray(b'{"ack":true}'))

        ack_task = asyncio.create_task(notify_ack())
        await transport._write_payload(client, payload, tracker)
        await ack_task

    asyncio.run(run())


def test_ble_state_status_reports_health_ages():
    state = BleState(
        connected=True,
        last_connected_at=10.0,
        last_write_at=12.0,
        last_ack_at=13.0,
        consecutive_failures=2,
        last_error="boom",
        last_payload=build_activity_payload(1, now=1),
    )

    status = state.status(queue_depth=3, now=15.0)

    assert status["connected"] is True
    assert status["queue_depth"] == 3
    assert status["last_connected_age_sec"] == 5.0
    assert status["last_write_age_sec"] == 3.0
    assert status["last_ack_age_sec"] == 2.0
    assert status["consecutive_failures"] == 2
    assert status["last_error"] == "boom"
    assert status["last_payload"] == "activity"
