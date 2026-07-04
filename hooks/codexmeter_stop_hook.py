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
        summary = summarize(str(hook_input.get("last_assistant_message") or ""))
        send_alert(summary, hook_input)
    except Exception:
        pass
    sys.stdout.write(json.dumps({"continue": True}, separators=(",", ":")))
    return 0


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


def send_alert(summary: str, hook_input: dict[str, Any]) -> None:
    socket_path = Path(os.environ.get("CODEXMETER_SOCKET", str(DEFAULT_SOCKET)))
    event = {
        "type": "alert",
        "title": "任务完成！",
        "body": summary,
        "source": "codex",
        "session_id": hook_input.get("session_id"),
        "turn_id": hook_input.get("turn_id"),
        "cwd": hook_input.get("cwd"),
    }
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
