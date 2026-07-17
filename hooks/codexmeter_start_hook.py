#!/usr/bin/env python3
"""Codex UserPromptSubmit hook that marks a Codex task as running."""

from __future__ import annotations

import json
import os
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
        send_task_start(hook_input)
    except Exception:
        pass
    sys.stdout.write(json.dumps({"continue": True}, separators=(",", ":")))
    return 0


def send_task_start(hook_input: dict[str, Any]) -> None:
    event = {
        "type": "task_start",
        "source": "codex",
        "session_id": hook_input.get("session_id"),
        "conversation_id": hook_input.get("conversation_id"),
        "turn_id": hook_input.get("turn_id"),
        "task_id": hook_input.get("task_id"),
        "cwd": hook_input.get("cwd"),
        "transcript_path": hook_input.get("transcript_path"),
    }
    send_event(event)


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
