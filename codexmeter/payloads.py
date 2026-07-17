"""BLE payload builders."""

from __future__ import annotations

import json
import re
import time
import unicodedata
import uuid
from dataclasses import dataclass
from typing import Any

from .limits import UsageSnapshot


MAX_BLE_PAYLOAD_BYTES = 512
MAX_ALERT_TITLE_BYTES = 47
MAX_ALERT_TITLE_CHARS = 16
MAX_ALERT_BODY_BYTES = 210
MAX_ALERT_BODY_CHARS = 64


@dataclass(frozen=True)
class Payload:
    kind: str
    data: dict[str, Any]

    def to_json_bytes(self) -> bytes:
        return encode_payload(self.data)


def build_usage_payload(snapshot: UsageSnapshot) -> Payload:
    data = {
        "v": 1,
        "k": "usage",
        "src": snapshot.source,
        "h5": snapshot.h5_remaining_percent,
        "h5r": snapshot.h5_resets_at,
        "d7": snapshot.d7_remaining_percent,
        "d7r": snapshot.d7_resets_at,
        "td": snapshot.today_tokens,
        "t7": snapshot.last_7d_tokens,
        "st": snapshot.status,
        "t": snapshot.generated_at,
    }
    return Payload("usage", data)


def build_activity_payload(
    running_count: int,
    source: str = "codex",
    now: int | None = None,
) -> Payload:
    data = {
        "v": 1,
        "k": "activity",
        "src": source,
        "run": max(0, int(running_count)),
        "t": int(now if now is not None else time.time()),
    }
    return Payload("activity", data)


def build_screen_control_payload(
    on: bool,
    reason: str,
    now: int | None = None,
) -> Payload:
    data = {
        "v": 1,
        "k": "control",
        "cmd": "screen",
        "on": bool(on),
        "why": sanitize_device_text(reason, fallback="auto"),
        "t": int(now if now is not None else time.time()),
    }
    return Payload("control", data)


def build_alert_payload(
    body: str,
    title: str = "任务完成！",
    event_id: str | None = None,
    now: int | None = None,
    running_count: int | None = None,
) -> Payload:
    title_text = truncate_text(
        sanitize_device_text(clean_summary(title), fallback="任务完成！"),
        max_chars=MAX_ALERT_TITLE_CHARS,
        max_bytes=MAX_ALERT_TITLE_BYTES,
    )
    body_text = truncate_text(
        sanitize_device_text(clean_summary(body)),
        max_chars=MAX_ALERT_BODY_CHARS,
        max_bytes=MAX_ALERT_BODY_BYTES,
    )
    data = {
        "v": 1,
        "k": "alert",
        "id": event_id or uuid.uuid4().hex[:12],
        "title": title_text,
        "body": body_text,
        "t": int(now if now is not None else time.time()),
    }
    if running_count is not None:
        data["run"] = max(0, int(running_count))
    return Payload("alert", data)


def encode_payload(data: dict[str, Any]) -> bytes:
    payload = json.dumps(data, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(payload) <= MAX_BLE_PAYLOAD_BYTES:
        return payload
    if data.get("k") != "alert":
        raise ValueError(f"BLE payload too large: {len(payload)} bytes")

    trimmed = dict(data)
    body = str(trimmed.get("body") or "")
    while body and len(payload) > MAX_BLE_PAYLOAD_BYTES:
        body = truncate_utf8(body, max(0, len(body.encode("utf-8")) - 24))
        trimmed["body"] = body
        payload = json.dumps(trimmed, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(payload) > MAX_BLE_PAYLOAD_BYTES:
        raise ValueError(f"BLE alert payload too large: {len(payload)} bytes")
    return payload


def clean_summary(text: str) -> str:
    compact = " ".join(line.strip() for line in text.splitlines() if line.strip())
    return compact or "Codex 任务已完成"


def sanitize_device_text(text: str, fallback: str = "Codex 任务已完成") -> str:
    """Remove control characters while preserving Unicode text for TinyTTF."""

    cleaned = "".join(ch if is_display_text_char(ch) else " " for ch in text)
    compact = re.sub(r"\s+", " ", cleaned).strip()
    return compact or fallback


def is_display_text_char(ch: str) -> bool:
    if ch in "\t\n\r":
        return False
    return unicodedata.category(ch)[0] != "C"


def truncate_utf8(text: str, max_bytes: int) -> str:
    raw = text.encode("utf-8")
    if len(raw) <= max_bytes:
        return text
    if max_bytes <= 1:
        return ""
    suffix = "..."
    keep = max_bytes - len(suffix)
    trimmed = raw[:keep]
    while trimmed:
        try:
            return trimmed.decode("utf-8") + suffix
        except UnicodeDecodeError:
            trimmed = trimmed[:-1]
    return suffix[:max_bytes]


def truncate_text(text: str, *, max_chars: int, max_bytes: int) -> str:
    if len(text) > max_chars:
        suffix = "..."
        text = text[: max(0, max_chars - len(suffix))].rstrip() + suffix
    return truncate_utf8(text, max_bytes)
