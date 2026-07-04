import json

from codexmeter.limits import UsageSnapshot
from codexmeter.payloads import (
    MAX_BLE_PAYLOAD_BYTES,
    build_activity_payload,
    build_alert_payload,
    build_screen_control_payload,
    build_usage_payload,
    sanitize_device_text,
)


def test_usage_payload_shape_and_size():
    payload = build_usage_payload(
        UsageSnapshot(
            source="codex",
            h5_remaining_percent=72,
            h5_resets_at=1783093200,
            d7_remaining_percent=84,
            d7_resets_at=1783545600,
            status="ok",
            generated_at=1783070000,
        )
    )
    encoded = payload.to_json_bytes()
    decoded = json.loads(encoded)
    assert len(encoded) <= MAX_BLE_PAYLOAD_BYTES
    assert decoded["k"] == "usage"
    assert decoded["h5"] == 72
    assert decoded["d7"] == 84


def test_activity_payload_shape_and_size():
    payload = build_activity_payload(3, now=1783070000)
    encoded = payload.to_json_bytes()
    decoded = json.loads(encoded)
    assert len(encoded) <= MAX_BLE_PAYLOAD_BYTES
    assert decoded == {
        "v": 1,
        "k": "activity",
        "src": "codex",
        "run": 3,
        "t": 1783070000,
    }


def test_alert_payload_truncates_to_ble_limit():
    payload = build_alert_payload("任务内容" * 200, now=1, event_id="abc")
    encoded = payload.to_json_bytes()
    decoded = json.loads(encoded)
    assert len(encoded) <= MAX_BLE_PAYLOAD_BYTES
    assert decoded["k"] == "alert"
    assert decoded["body"].endswith("...")


def test_alert_payload_sanitizes_unsupported_device_text():
    payload = build_alert_payload("修复任务信息乱码🚀𠮷成功", now=1, event_id="abc")
    decoded = json.loads(payload.to_json_bytes())
    assert decoded["body"] == "修复任务信息乱码 成功"


def test_alert_payload_can_carry_running_count():
    payload = build_alert_payload("摘要", now=1, event_id="abc", running_count=0)
    decoded = json.loads(payload.to_json_bytes())
    assert decoded["k"] == "alert"
    assert decoded["run"] == 0
    assert len(payload.to_json_bytes()) <= MAX_BLE_PAYLOAD_BYTES


def test_screen_control_payload_shape_and_size():
    payload = build_screen_control_payload(False, reason="mac_locked", now=1783070000)
    encoded = payload.to_json_bytes()
    decoded = json.loads(encoded)
    assert len(encoded) <= MAX_BLE_PAYLOAD_BYTES
    assert decoded == {
        "v": 1,
        "k": "control",
        "cmd": "screen",
        "on": False,
        "why": "mac_locked",
        "t": 1783070000,
    }


def test_sanitize_device_text_falls_back_when_empty():
    assert sanitize_device_text("🚀𠮷") == "Codex 任务已完成"
