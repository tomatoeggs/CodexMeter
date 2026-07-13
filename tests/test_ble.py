import asyncio

from codexmeter.ble import (
    BleAckError,
    BleAckTimeout,
    BleAckTracker,
    BleConnectTimeout,
    BleIdentity,
    BleIdentityError,
    BleNotifyError,
    BleState,
    BleTransport,
    BleWriteTimeout,
)
from codexmeter.payloads import (
    build_activity_payload,
    build_alert_payload,
    build_screen_control_payload,
)


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


class FakeLifecycleClient:
    def __init__(
        self,
        *,
        connect_delay_sec: float = 0.0,
        disconnect_delay_sec: float = 0.0,
    ) -> None:
        self.connect_delay_sec = connect_delay_sec
        self.disconnect_delay_sec = disconnect_delay_sec
        self.disconnect_calls = 0
        self.is_connected = False

    async def connect(self) -> None:
        await asyncio.sleep(self.connect_delay_sec)
        self.is_connected = True

    async def disconnect(self) -> None:
        self.disconnect_calls += 1
        await asyncio.sleep(self.disconnect_delay_sec)
        self.is_connected = False


class FakeClientTransport(BleTransport):
    def __init__(self, client: FakeLifecycleClient, **kwargs) -> None:
        super().__init__(**kwargs)
        self.client = client

    def _create_client(self, _target):
        return self.client


def test_ble_connect_timeout_disconnects_partial_client():
    async def run() -> None:
        client = FakeLifecycleClient(connect_delay_sec=1.0)
        transport = FakeClientTransport(
            client,
            connect_timeout_sec=0.01,
            disconnect_timeout_sec=0.1,
        )
        state = BleState()

        try:
            await transport.connect_and_write(
                object(), asyncio.Queue(), state, asyncio.Event()
            )
        except BleConnectTimeout:
            pass
        else:
            raise AssertionError("expected BLE connect timeout")

        assert client.disconnect_calls == 1
        assert state.connected is False
        assert state.last_disconnected_at is None

    asyncio.run(run())


def test_ble_disconnect_timeout_bounds_cleanup():
    async def run() -> None:
        client = FakeLifecycleClient(disconnect_delay_sec=1.0)
        transport = BleTransport(disconnect_timeout_sec=0.01)

        await transport._disconnect(client, "test-device")

        assert client.disconnect_calls == 1

    asyncio.run(run())


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


def test_ble_connected_lookup_requires_expected_device_name():
    transport = BleTransport(device_name="CodexMeter")

    assert transport._connected_name_matches("CodexMeter") is True
    assert transport._connected_name_matches("Clawdmeter") is False
    assert transport._connected_name_matches(None) is False


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


def test_ble_write_payload_rejects_device_nack_without_clearing_failure_state():
    async def run() -> None:
        transport = BleTransport(write_timeout_sec=0.1, ack_timeout_sec=0.1)
        client = FakeWriteClient()
        tracker = BleAckTracker()
        state = BleState(consecutive_failures=2, last_error="previous")
        payload = build_activity_payload(1, now=1)

        async def notify_nack() -> None:
            await asyncio.sleep(0)
            tracker.notify(bytearray(b'{"err":true}'))

        ack_task = asyncio.create_task(notify_nack())
        try:
            await transport._write_payload(client, payload, tracker, state)
        except BleAckError as exc:
            assert "NACK" in str(exc)
        else:
            raise AssertionError("expected BLE NACK error")
        await ack_task
        assert state.last_payload is None
        assert state.last_ack_at is None
        assert state.consecutive_failures == 2
        assert state.last_error == "previous"

    asyncio.run(run())


def test_ble_write_payload_rejects_invalid_ack_json():
    async def run() -> None:
        transport = BleTransport(write_timeout_sec=0.1, ack_timeout_sec=0.1)
        client = FakeWriteClient()
        tracker = BleAckTracker()
        payload = build_activity_payload(1, now=1)

        async def notify_invalid_ack() -> None:
            await asyncio.sleep(0)
            tracker.notify(bytearray(b"not-json"))

        ack_task = asyncio.create_task(notify_invalid_ack())
        try:
            await transport._write_payload(client, payload, tracker)
        except BleAckError as exc:
            assert "invalid" in str(exc)
        else:
            raise AssertionError("expected BLE ACK parse error")
        await ack_task

    asyncio.run(run())


def test_ble_state_status_reports_health_ages():
    state = BleState(
        connected=True,
        last_discovered_at=9.0,
        last_discovery_source="connected",
        last_discovered_address="device-address",
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
    assert status["last_discovered_age_sec"] == 6.0
    assert status["last_discovery_source"] == "connected"
    assert status["last_discovered_address"] == "device-address"
    assert status["last_connected_age_sec"] == 5.0
    assert status["last_write_age_sec"] == 3.0
    assert status["last_ack_age_sec"] == 2.0
    assert status["consecutive_failures"] == 2
    assert status["last_error"] == "boom"
    assert status["last_payload"] == "activity"


def test_ble_identity_parses_required_fields():
    identity = BleIdentity.from_bytes(
        b'{"device_id":"codexmeter-f46690858428","short_id":"F46690",'
        b'"name":"CodexMeter-F46690"}'
    )

    assert identity.device_id == "codexmeter-f46690858428"
    assert identity.short_id == "F46690"


def test_ble_identity_rejects_incomplete_json():
    try:
        BleIdentity.from_bytes(b'{"short_id":"F46690"}')
    except BleIdentityError:
        return
    raise AssertionError("expected incomplete BLE identity to be rejected")


def test_ble_identity_rejects_device_id_short_id_mismatch():
    try:
        BleIdentity.from_bytes(
            b'{"device_id":"codexmeter-a3f91c858428","short_id":"F46690",'
            b'"name":"CodexMeter-F46690"}'
        )
    except BleIdentityError:
        return
    raise AssertionError("expected inconsistent BLE identity to be rejected")


def test_ble_failed_alert_remains_pending_until_ack():
    async def run() -> None:
        transport = BleTransport(write_timeout_sec=0.01, ack_timeout_sec=0.01)
        state = BleState()
        payload = build_alert_payload("reliable", event_id="alert-1", now=1)

        try:
            await transport._write_reliably(
                FakeWriteClient(delay_sec=1.0), payload, BleAckTracker(), state
            )
        except BleWriteTimeout:
            pass
        else:
            raise AssertionError("expected BLE write timeout")
        assert state.pending_alert == payload

    asyncio.run(run())


def test_ble_remembers_latest_desired_state_before_ack():
    state = BleState()
    payload = build_activity_payload(3, now=1)

    BleTransport.remember_desired(payload, state)

    assert state.last_activity == payload
