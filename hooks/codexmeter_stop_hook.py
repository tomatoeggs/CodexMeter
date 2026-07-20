#!/usr/bin/env python3
"""Codex Stop hook that notifies the local CodexMeter daemon.

The hook must never block Codex completion. Any socket or JSON error exits
successfully with a tiny JSON response accepted by the Stop hook contract.
"""

from __future__ import annotations

import json
import os
import re
import socket
import sys
from pathlib import Path
from typing import Any


DEFAULT_SOCKET = Path.home() / ".codexmeter" / "events.sock"


def main() -> int:
    try:
        hook_input = json.load(sys.stdin)
        if not isinstance(hook_input, dict):
            hook_input = {}
    except Exception:
        hook_input = {}

    try:
        notify_stop(hook_input)
    except Exception:
        pass
    sys.stdout.write(json.dumps({"continue": True}, separators=(",", ":")))
    return 0


def notify_stop(hook_input: dict[str, Any]) -> None:
    message = str(hook_input.get("last_assistant_message") or "")
    if looks_like_task_descriptor(message):
        send_task_finish(hook_input)
        return
    send_task_complete(summarize(message), hook_input)


def summarize(message: str, max_chars: int = 96) -> str:
    text = re.sub(r"```.*?```", " ", message, flags=re.S)
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"[*_#>\-]+", " ", text)
    text = " ".join(part.strip() for part in text.splitlines() if part.strip())
    text = re.sub(r"\s+", " ", text).strip()
    if not text:
        text = "Codex 任务已完成"
    if len(text) <= max_chars:
        return text
    return text[: max_chars - 3].rstrip() + "..."


def looks_like_task_descriptor(message: str) -> bool:
    try:
        value = json.loads(message.strip())
    except Exception:
        return False
    if not isinstance(value, dict) or set(value) != {"title", "description"}:
        return False
    title = value.get("title")
    description = value.get("description")
    if not isinstance(title, str) or not isinstance(description, str):
        return False
    title = title.strip()
    description = description.strip()
    return bool(title and description and len(title) <= 120 and len(description) <= 400)


def send_task_finish(hook_input: dict[str, Any]) -> None:
    event = build_task_event("task_finish", hook_input)
    event["allow_oldest_fallback"] = False
    send_event(event)


def send_task_complete(summary: str, hook_input: dict[str, Any]) -> None:
    event = build_task_event("task_complete", hook_input)
    event.update(
        {
            "title": "任务完成！",
            "body": summary,
        }
    )
    send_event(event)


def build_task_event(event_type: str, hook_input: dict[str, Any]) -> dict[str, Any]:
    return {
        "type": event_type,
        "source": "codex",
        "session_id": hook_input.get("session_id"),
        "conversation_id": hook_input.get("conversation_id"),
        "turn_id": hook_input.get("turn_id"),
        "task_id": hook_input.get("task_id"),
        "cwd": hook_input.get("cwd"),
    }


def send_event(event: dict[str, Any]) -> None:
    socket_path = Path(os.environ.get("CODEXMETER_SOCKET", str(DEFAULT_SOCKET)))
    payload = json.dumps(event, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(0.35)
        client.connect(str(socket_path))
        client.sendall(payload)
        with contextlib_suppress_timeout():
            client.shutdown(socket.SHUT_WR)
            client.recv(256)


class contextlib_suppress_timeout:
    def __enter__(self) -> None:
        return None

    def __exit__(self, exc_type: Any, exc: BaseException | None, _tb: Any) -> bool:
        return isinstance(exc, (TimeoutError, OSError))


if __name__ == "__main__":
    raise SystemExit(main())
