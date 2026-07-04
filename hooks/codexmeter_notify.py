#!/usr/bin/env python3
"""Codex notify wrapper that also sends a CodexMeter completion alert."""

from __future__ import annotations

import json
import os
import re
import socket
import subprocess
import sys
from pathlib import Path
from typing import Any


DEFAULT_SOCKET = Path.home() / ".codexmeter" / "events.sock"


def main() -> int:
    raw_stdin = sys.stdin.buffer.read()
    original = split_original_command(sys.argv[1:])
    try:
        summary = summarize(notification_text(raw_stdin))
        send_alert(summary)
    except Exception:
        pass
    run_original(original, raw_stdin)
    return 0


def split_original_command(args: list[str]) -> list[str]:
    if args and args[0] == "--":
        return args[1:]
    return args


def notification_text(raw_stdin: bytes) -> str:
    if not raw_stdin:
        return ""
    try:
        payload = json.loads(raw_stdin.decode("utf-8"))
    except Exception:
        return raw_stdin.decode("utf-8", errors="ignore")
    if not isinstance(payload, dict):
        return ""
    for key in ("last_assistant_message", "message", "summary", "text"):
        value = payload.get(key)
        if isinstance(value, str) and value.strip():
            return value
    return ""


def summarize(message: str, max_chars: int = 96) -> str:
    text = re.sub(r"```.*?```", " ", message or "", flags=re.S)
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"[*_#>\-]+", " ", text)
    text = " ".join(part.strip() for part in text.splitlines() if part.strip())
    text = re.sub(r"\s+", " ", text).strip()
    if not text:
        text = "Codex 任务已完成"
    if len(text) <= max_chars:
        return text
    return text[: max_chars - 3].rstrip() + "..."


def send_alert(summary: str) -> None:
    socket_path = Path(os.environ.get("CODEXMETER_SOCKET", str(DEFAULT_SOCKET)))
    event: dict[str, Any] = {
        "type": "alert",
        "title": "任务完成！",
        "body": summary,
        "source": "codex-notify",
    }
    payload = json.dumps(event, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(0.35)
        client.connect(str(socket_path))
        client.sendall(payload)
        try:
            client.shutdown(socket.SHUT_WR)
            client.recv(256)
        except OSError:
            pass


def run_original(command: list[str], raw_stdin: bytes) -> None:
    if not command:
        return
    try:
        subprocess.run(
            command,
            input=raw_stdin,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5,
            check=False,
        )
    except Exception:
        pass


if __name__ == "__main__":
    raise SystemExit(main())
