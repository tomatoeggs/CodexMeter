import json

from codexmeter.limits import UsageSnapshot
from codexmeter.payloads import (
    MAX_BLE_PAYLOAD_BYTES,
    Payload,
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
            today_tokens=18_600_000,
            last_7d_tokens=236_000_000,
        )
    )
    encoded = payload.to_json_bytes()
    decoded = json.loads(encoded)
    assert len(encoded) <= MAX_BLE_PAYLOAD_BYTES
    assert decoded["k"] == "usage"
    assert decoded["h5"] == 72
    assert decoded["d7"] == 84
    assert decoded["td"] == 18_600_000
    assert decoded["t7"] == 236_000_000


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


def test_alert_payload_preserves_unicode_text_for_tiny_ttf():
    payload = build_alert_payload("修复任务信息乱码🚀𠮷成功", now=1, event_id="abc")
    decoded = json.loads(payload.to_json_bytes())
    assert decoded["body"] == "修复任务信息乱码🚀𠮷成功"


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
    assert sanitize_device_text("\n\t\r") == "Codex 任务已完成"


def test_alert_payload_can_use_full_512_byte_protocol_limit():
    payload = Payload(
        "alert",
        {
            "v": 1,
            "k": "alert",
            "id": "123456789abc",
            "title": "t" * 227,
            "body": "x" * 210,
            "t": 1783070000,
        },
    )

    assert len(payload.to_json_bytes()) == 512


def test_alert_builder_caps_text_complexity_for_device_rendering():
    payload = build_alert_payload(
        "x" * 210,
        title="t" * 227,
        event_id="render-cap",
        now=1783070000,
    )

    assert len(payload.data["title"]) == 16
    assert len(payload.data["body"]) == 64
    assert payload.data["title"].endswith("...")
    assert payload.data["body"].endswith("...")
