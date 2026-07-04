from codexmeter.ble import BleState, BleTransport
from codexmeter.payloads import build_screen_control_payload


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
