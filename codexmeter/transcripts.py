"""Best-effort detection of completed or interrupted Codex turns."""

from __future__ import annotations

import asyncio
import json
import logging
import os
from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

log = logging.getLogger(__name__)

TurnEnded = Callable[[str, str], Awaitable[None]]
TurnsActive = Callable[[set[str]], None]

_END_EVENT_TYPES = frozenset({"task_complete", "turn_aborted"})
_MAX_READ_BYTES = 1024 * 1024
_MAX_LINE_BYTES = 1024 * 1024


@dataclass
class _Cursor:
    path: Path
    offset: int
    turn_ids: set[str] = field(default_factory=set)
    buffer: bytes = b""
    discard_until_newline: bool = False

    def reset(self) -> None:
        self.offset = 0
        self.buffer = b""
        self.discard_until_newline = False

    def feed(self, data: bytes) -> list[bytes]:
        if self.discard_until_newline:
            newline = data.find(b"\n")
            if newline < 0:
                return []
            data = data[newline + 1 :]
            self.discard_until_newline = False

        self.buffer += data
        lines: list[bytes] = []
        while True:
            newline = self.buffer.find(b"\n")
            if newline < 0:
                break
            line = self.buffer[:newline]
            self.buffer = self.buffer[newline + 1 :]
            if len(line) <= _MAX_LINE_BYTES:
                lines.append(line)

        if len(self.buffer) > _MAX_LINE_BYTES:
            self.buffer = b""
            self.discard_until_newline = True
        return lines


class TranscriptWatcher:
    """Incrementally watch trusted Codex transcripts for turn-end events.

    Transcript rows are intentionally treated as an optional acceleration signal.
    Missing files, malformed rows, and unknown schemas are ignored.
    """

    def __init__(
        self,
        turn_ended: TurnEnded,
        codex_home: Path | None = None,
        poll_interval: float = 1.0,
        turns_active: TurnsActive | None = None,
    ) -> None:
        configured_home = os.environ.get("CODEX_HOME")
        self.codex_home = Path(
            codex_home
            if codex_home is not None
            else configured_home or Path.home() / ".codex"
        ).expanduser()
        self.poll_interval = max(0.1, poll_interval)
        self.turn_ended = turn_ended
        self.turns_active = turns_active
        self._cursors: dict[Path, _Cursor] = {}
        self._paths_by_turn: dict[str, Path] = {}

    @property
    def watch_count(self) -> int:
        return len(self._paths_by_turn)

    @property
    def file_count(self) -> int:
        return len(self._cursors)

    def watch(self, turn_id: object, transcript_path: object) -> bool:
        if not isinstance(turn_id, str) or not turn_id.strip():
            return False
        path = self._trusted_path(transcript_path)
        if path is None:
            return False

        normalized_turn_id = turn_id.strip()
        current_path = self._paths_by_turn.get(normalized_turn_id)
        if current_path == path:
            return True
        if current_path is not None:
            self.unwatch(normalized_turn_id)

        cursor = self._cursors.get(path)
        if cursor is None:
            try:
                offset = path.stat().st_size
            except OSError:
                offset = 0
            cursor = _Cursor(path=path, offset=offset)
            self._cursors[path] = cursor
        cursor.turn_ids.add(normalized_turn_id)
        self._paths_by_turn[normalized_turn_id] = path
        log.debug("Watching transcript turn=%s path=%s", normalized_turn_id, path)
        return True

    def unwatch(self, turn_id: str) -> None:
        path = self._paths_by_turn.pop(turn_id, None)
        if path is None:
            return
        cursor = self._cursors.get(path)
        if cursor is None:
            return
        cursor.turn_ids.discard(turn_id)
        if not cursor.turn_ids:
            del self._cursors[path]

    def clear(self) -> None:
        self._cursors.clear()
        self._paths_by_turn.clear()

    async def run(self, stop_event: asyncio.Event) -> None:
        while not stop_event.is_set():
            try:
                await self.scan_once()
            except asyncio.CancelledError:
                raise
            except Exception:
                log.exception("Transcript watcher scan failed")
            try:
                await asyncio.wait_for(stop_event.wait(), timeout=self.poll_interval)
            except asyncio.TimeoutError:
                pass

    async def scan_once(self) -> None:
        for path in tuple(self._cursors):
            cursor = self._cursors.get(path)
            if cursor is None:
                continue
            rows, advanced = self._read_rows(cursor)
            if advanced and self.turns_active is not None:
                self.turns_active(set(cursor.turn_ids))
            for row in rows:
                ended = _parse_turn_end(row)
                if ended is None:
                    continue
                turn_id, event_type = ended
                if turn_id not in cursor.turn_ids:
                    continue
                self.unwatch(turn_id)
                log.info(
                    "Detected transcript turn end turn=%s event=%s",
                    turn_id,
                    event_type,
                )
                await self.turn_ended(turn_id, event_type)

    def _read_rows(self, cursor: _Cursor) -> tuple[list[bytes], bool]:
        try:
            size = cursor.path.stat().st_size
            if size < cursor.offset:
                cursor.reset()
            with cursor.path.open("rb") as handle:
                handle.seek(cursor.offset)
                data = handle.read(_MAX_READ_BYTES)
        except OSError:
            return [], False
        if not data:
            return [], False
        cursor.offset += len(data)
        return cursor.feed(data), True

    def _trusted_path(self, value: object) -> Path | None:
        if not isinstance(value, str) or not value.strip():
            return None
        try:
            root = self.codex_home.resolve()
            path = Path(value).expanduser().resolve()
            path.relative_to(root)
        except (OSError, RuntimeError, ValueError):
            return None
        return path


def _parse_turn_end(line: bytes) -> tuple[str, str] | None:
    if b"task_complete" not in line and b"turn_aborted" not in line:
        return None
    try:
        row = json.loads(line)
    except (json.JSONDecodeError, UnicodeDecodeError):
        return None
    if not isinstance(row, dict) or row.get("type") != "event_msg":
        return None
    payload = row.get("payload")
    if not isinstance(payload, dict):
        return None
    event_type = payload.get("type")
    turn_id = payload.get("turn_id")
    if event_type not in _END_EVENT_TYPES:
        return None
    if not isinstance(turn_id, str) or not turn_id.strip():
        return None
    return turn_id.strip(), event_type
