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
INTERNAL_THREAD_SOURCES = frozenset({"subagent"})
MAX_META_SCAN_LINES = 64
MAX_META_LINE_BYTES = 1024 * 1024


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
    if not should_track_task(hook_input):
        return
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


def should_track_task(hook_input: dict[str, Any]) -> bool:
    if is_internal_task_context(hook_input):
        return False
    metadata = read_transcript_metadata(hook_input.get("transcript_path"))
    if metadata is not None and is_internal_task_context(metadata):
        return False
    return True


def is_internal_task_context(context: dict[str, Any]) -> bool:
    thread_source = context.get("thread_source")
    if (
        isinstance(thread_source, str)
        and thread_source.strip().lower() in INTERNAL_THREAD_SOURCES
    ):
        return True

    source = context.get("source")
    return isinstance(source, dict) and source.get("subagent") is not None


def read_transcript_metadata(transcript_path: object) -> dict[str, Any] | None:
    path = trusted_transcript_path(transcript_path)
    if path is None:
        return None
    try:
        with path.open("rb") as handle:
            for _ in range(MAX_META_SCAN_LINES):
                line = handle.readline(MAX_META_LINE_BYTES + 1)
                if not line:
                    break
                if len(line) > MAX_META_LINE_BYTES or b"session_meta" not in line:
                    continue
                row = json.loads(line)
                if not isinstance(row, dict) or row.get("type") != "session_meta":
                    continue
                payload = row.get("payload")
                return payload if isinstance(payload, dict) else None
    except (OSError, json.JSONDecodeError, UnicodeDecodeError):
        return None
    return None


def trusted_transcript_path(value: object) -> Path | None:
    if not isinstance(value, str) or not value.strip():
        return None
    try:
        configured_home = os.environ.get("CODEX_HOME", str(Path.home() / ".codex"))
        codex_home = Path(configured_home).expanduser().resolve()
        path = Path(value).expanduser().resolve()
        path.relative_to(codex_home)
    except (OSError, RuntimeError, ValueError):
        return None
    return path


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
