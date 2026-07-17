"""Read near-real-time token activity from local Codex session events."""

from __future__ import annotations

import datetime as dt
import json
import os
from dataclasses import dataclass
from pathlib import Path


@dataclass
class _SessionFileState:
    inode: int
    offset: int = 0
    previous_total: int | None = None
    today_tokens: int = 0
    saw_today_event: bool = False
    replay_until: dt.datetime | None = None


class LocalTokenUsageReader:
    """Incrementally sum today's token deltas without reading message bodies."""

    def __init__(self, codex_home: Path | None = None) -> None:
        configured_home = os.environ.get("CODEX_HOME")
        self.codex_home = Path(
            codex_home
            if codex_home is not None
            else configured_home or Path.home() / ".codex"
        )
        self.sessions_root = self.codex_home / "sessions"
        self._date: dt.date | None = None
        self._states: dict[Path, _SessionFileState] = {}

    def read_today_tokens(self, now: dt.datetime | None = None) -> int | None:
        local_now = now if now is not None else dt.datetime.now().astimezone()
        if local_now.tzinfo is None:
            local_now = local_now.astimezone()
        target_date = local_now.date()
        if target_date != self._date:
            self._date = target_date
            self._states.clear()

        if not self.sessions_root.is_dir():
            return None

        day_start = dt.datetime.combine(
            target_date,
            dt.time.min,
            tzinfo=local_now.tzinfo,
        ).timestamp()
        candidates: set[Path] = set()
        try:
            paths = self.sessions_root.rglob("*.jsonl")
            for path in paths:
                try:
                    stat = path.stat()
                except OSError:
                    continue
                if stat.st_mtime < day_start:
                    continue
                candidates.add(path)
                self._scan_file(path, stat.st_ino, target_date, local_now.tzinfo)
        except OSError:
            return None

        for path in tuple(self._states):
            if path not in candidates:
                del self._states[path]

        observed = [state for state in self._states.values() if state.saw_today_event]
        if not observed:
            return None
        return sum(state.today_tokens for state in observed)

    def _scan_file(
        self,
        path: Path,
        inode: int,
        target_date: dt.date,
        timezone: dt.tzinfo,
    ) -> None:
        state = self._states.get(path)
        try:
            size = path.stat().st_size
        except OSError:
            return
        if state is None or state.inode != inode or size < state.offset:
            state = _SessionFileState(inode=inode)
            self._states[path] = state

        try:
            with path.open(encoding="utf-8") as handle:
                handle.seek(state.offset)
                while True:
                    line_start = handle.tell()
                    line = handle.readline()
                    if not line:
                        break
                    if not line.endswith("\n"):
                        handle.seek(line_start)
                        break
                    state.offset = handle.tell()
                    self._capture_session_meta(line, state)
                    if '"type":"token_count"' not in line:
                        continue
                    self._apply_token_event(line, state, target_date, timezone)
        except OSError:
            return

    @staticmethod
    def _capture_session_meta(line: str, state: _SessionFileState) -> None:
        if state.replay_until is not None or '"type":"session_meta"' not in line:
            return
        try:
            row = json.loads(line)
            payload = row.get("payload")
            timestamp = row.get("timestamp")
            if row.get("type") != "session_meta" or not isinstance(payload, dict):
                return
            if not payload.get("forked_from_id") or not isinstance(timestamp, str):
                return
            started_at = dt.datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
            if started_at.tzinfo is None:
                started_at = started_at.replace(tzinfo=dt.timezone.utc)
        except (json.JSONDecodeError, TypeError, ValueError):
            return
        # Forked sessions replay the parent's token_count history immediately
        # after session_meta. Treat that burst as a baseline, not fresh usage.
        state.replay_until = started_at + dt.timedelta(seconds=1)

    @staticmethod
    def _apply_token_event(
        line: str,
        state: _SessionFileState,
        target_date: dt.date,
        timezone: dt.tzinfo,
    ) -> None:
        try:
            row = json.loads(line)
            payload = row.get("payload")
            if row.get("type") != "event_msg" or not isinstance(payload, dict):
                return
            if payload.get("type") != "token_count":
                return
            info = payload.get("info")
            if not isinstance(info, dict):
                return
            total_usage = info.get("total_token_usage")
            if not isinstance(total_usage, dict):
                return
            current_total = total_usage.get("total_tokens")
            timestamp = row.get("timestamp")
            if isinstance(current_total, bool) or not isinstance(current_total, int):
                return
            if not isinstance(timestamp, str):
                return
            event_time = dt.datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
            if event_time.tzinfo is None:
                event_time = event_time.replace(tzinfo=dt.timezone.utc)
        except (json.JSONDecodeError, TypeError, ValueError):
            return

        is_replayed_baseline = (
            state.replay_until is not None and event_time <= state.replay_until
        )
        if event_time.astimezone(timezone).date() == target_date and not is_replayed_baseline:
            if state.previous_total is None or current_total < state.previous_total:
                delta = current_total
            else:
                delta = current_total - state.previous_total
            state.today_tokens += max(0, delta)
            state.saw_today_event = True
        state.previous_total = current_total
