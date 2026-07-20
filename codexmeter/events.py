"""Local Unix-socket event server used by Codex hooks and ctl commands."""

from __future__ import annotations

import asyncio
import json
import logging
import time
from collections import deque
from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .payloads import (
    Payload,
    build_activity_payload,
    build_alert_payload,
    build_screen_control_payload,
)
from .settings import ACTIVITY_SWEEP_INTERVAL_SEC, ACTIVITY_TTL_SEC, EVENT_SOCKET
from .transcripts import TranscriptWatcher

log = logging.getLogger(__name__)

PayloadSink = Callable[[Payload], Awaitable[None]]
StatusProvider = Callable[[], dict[str, Any]]


TASK_ID_FIELDS = ("task_id", "turn_id", "session_id", "conversation_id")
IDENTITY_TASK_ID_FIELDS = ("turn_id", "task_id")
FINISHED_TASK_HISTORY = 128


@dataclass
class ActiveTask:
    aliases: set[str]
    started_at: float = field(default_factory=time.monotonic)
    last_seen_at: float = field(default_factory=time.monotonic)


def task_aliases(event: dict[str, Any]) -> list[str]:
    aliases: list[str] = []
    for key in TASK_ID_FIELDS:
        value = event.get(key)
        if isinstance(value, str) and value.strip():
            aliases.append(f"{key}:{value.strip()}")
    return aliases


@dataclass
class ActivityTracker:
    active_tasks: dict[str, ActiveTask] = field(default_factory=dict)
    finished_tasks: deque[frozenset[str]] = field(
        default_factory=lambda: deque(maxlen=FINISHED_TASK_HISTORY)
    )
    anonymous_seq: int = 0
    last_changed_at: float = field(default_factory=time.monotonic)

    @property
    def count(self) -> int:
        return len(self.active_tasks)

    def start(self, event: dict[str, Any]) -> bool:
        before = self.count
        aliases = task_aliases(event)
        identity_aliases = _identity_aliases(aliases)
        if identity_aliases and self._was_finished(identity_aliases):
            return False
        existing_key = self._find_task(aliases)
        if existing_key is not None:
            task = self.active_tasks[existing_key]
            task.aliases.update(aliases)
            task.last_seen_at = time.monotonic()
            return self._changed(before)

        if identity_aliases:
            primary = identity_aliases[0]
        elif aliases:
            primary = aliases[0]
        else:
            primary = self._anonymous_key()
        started_at = time.monotonic()
        self.active_tasks[primary] = ActiveTask(
            set(aliases or [primary]),
            started_at=started_at,
            last_seen_at=started_at,
        )
        return self._changed(before)

    def finish(self, event: dict[str, Any]) -> bool:
        return self.finish_task(event) is not None

    def finish_task(
        self,
        event: dict[str, Any],
        *,
        allow_oldest_fallback: bool = True,
    ) -> ActiveTask | None:
        before = self.count
        aliases = task_aliases(event)
        identity_aliases = _identity_aliases(aliases)
        if self._was_finished(identity_aliases or aliases):
            return None
        matched_key = self._find_task(
            aliases, allow_context_fallback=allow_oldest_fallback
        )
        if (
            matched_key is None
            and self.active_tasks
            and allow_oldest_fallback
            and not identity_aliases
        ):
            matched_key = self._oldest_task()
        if matched_key is None:
            return None
        task = self.active_tasks.pop(matched_key)
        self.finished_tasks.append(frozenset(task.aliases | set(aliases)))
        self._changed(before)
        return task

    def set_count(self, count: int) -> bool:
        before = self.count
        self.active_tasks.clear()
        self.finished_tasks.clear()
        for _ in range(max(0, int(count))):
            key = self._anonymous_key()
            self.active_tasks[key] = ActiveTask({key})
        return self._changed(before)

    def touch_turns(self, turn_ids: set[str], now: float | None = None) -> int:
        aliases = {f"turn_id:{turn_id}" for turn_id in turn_ids if turn_id}
        if not aliases:
            return 0
        observed_at = time.monotonic() if now is None else now
        touched = 0
        for task in self.active_tasks.values():
            if task.aliases & aliases:
                task.last_seen_at = observed_at
                touched += 1
        return touched

    def expire_stale(
        self,
        ttl_sec: float,
        now: float | None = None,
    ) -> list[ActiveTask]:
        if ttl_sec <= 0:
            return []
        checked_at = time.monotonic() if now is None else now
        stale_keys = [
            key
            for key, task in self.active_tasks.items()
            if checked_at - task.last_seen_at >= ttl_sec
        ]
        if not stale_keys:
            return []

        before = self.count
        expired = [self.active_tasks.pop(key) for key in stale_keys]
        for task in expired:
            self.finished_tasks.append(frozenset(task.aliases))
        self._changed(before)
        return expired

    def _find_task(
        self,
        aliases: list[str],
        *,
        allow_context_fallback: bool = False,
    ) -> str | None:
        if not aliases:
            return None
        identity_aliases = _identity_aliases(aliases)
        alias_set = set(identity_aliases or aliases)
        for key, task in self.active_tasks.items():
            if task.aliases & alias_set:
                return key
        if not identity_aliases or not allow_context_fallback:
            return None

        context_aliases = set(aliases) - set(identity_aliases)
        context_matches = [
            key
            for key, task in self.active_tasks.items()
            if task.aliases & context_aliases
        ]
        if len(context_matches) == 1:
            return context_matches[0]
        return None

    def _was_finished(self, aliases: list[str]) -> bool:
        alias_set = set(aliases)
        return bool(alias_set) and any(
            finished & alias_set for finished in self.finished_tasks
        )

    def _oldest_task(self) -> str:
        return min(self.active_tasks, key=lambda key: self.active_tasks[key].started_at)

    def _anonymous_key(self) -> str:
        self.anonymous_seq += 1
        return f"anonymous:{self.anonymous_seq}"

    def _changed(self, before: int) -> bool:
        changed = before != self.count
        if changed:
            self.last_changed_at = time.monotonic()
        return changed


class EventServer:
    def __init__(
        self,
        sink: PayloadSink,
        socket_path: Path = EVENT_SOCKET,
        status_provider: StatusProvider | None = None,
        transcript_root: Path | None = None,
        transcript_poll_interval: float = 1.0,
        activity_ttl: float = ACTIVITY_TTL_SEC,
        activity_sweep_interval: float = ACTIVITY_SWEEP_INTERVAL_SEC,
        verify_task_starts: bool = False,
    ) -> None:
        self.sink = sink
        self.socket_path = socket_path
        self.status_provider = status_provider
        self.server: asyncio.AbstractServer | None = None
        self.activity = ActivityTracker()
        self.verify_task_starts = verify_task_starts
        self.activity_ttl = max(0.0, activity_ttl)
        self.activity_sweep_interval = max(0.1, activity_sweep_interval)
        self.transcripts = TranscriptWatcher(
            self._finish_transcript_turn,
            codex_home=transcript_root,
            poll_interval=transcript_poll_interval,
            turns_active=self.activity.touch_turns,
        )

    async def run(self, stop_event: asyncio.Event) -> None:
        self.socket_path.parent.mkdir(parents=True, exist_ok=True)
        if self.socket_path.exists():
            self.socket_path.unlink()
        self.server = await asyncio.start_unix_server(self._handle_client, self.socket_path)
        log.info("Listening for Codex events on %s", self.socket_path)
        background_tasks = [
            asyncio.create_task(self.transcripts.run(stop_event), name="transcripts")
        ]
        if self.activity_ttl > 0:
            background_tasks.append(
                asyncio.create_task(
                    self._activity_ttl_loop(stop_event), name="activity-ttl"
                )
            )
        try:
            async with self.server:
                await stop_event.wait()
        finally:
            for task in background_tasks:
                task.cancel()
            await asyncio.gather(*background_tasks, return_exceptions=True)
            if self.server is not None:
                self.server.close()
                await self.server.wait_closed()
            if self.socket_path.exists():
                self.socket_path.unlink()

    async def _handle_client(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        try:
            raw = await asyncio.wait_for(reader.read(4096), timeout=2)
            event = json.loads(raw.decode("utf-8"))
            if not isinstance(event, dict):
                raise ValueError("event must be a JSON object")
            response = await self._dispatch(event)
        except Exception as exc:
            log.debug("Rejected local event: %s", exc)
            response = {"ok": False, "error": str(exc)}
        writer.write(json.dumps(response, separators=(",", ":")).encode("utf-8"))
        await writer.drain()
        writer.close()
        await writer.wait_closed()

    async def _dispatch(self, event: dict[str, Any]) -> dict[str, Any]:
        event_type = event.get("type")
        if event_type == "ping":
            response: dict[str, Any] = {
                "ok": True,
                "status": "running",
                "activity": self._activity_status(
                    include_tasks=bool(event.get("include_activity_tasks"))
                ),
            }
            if self.status_provider is not None:
                response.update(self.status_provider())
            return response
        if event_type == "alert":
            body = str(event.get("body") or event.get("summary") or "")
            title = str(event.get("title") or "任务完成！")
            running_count = event.get("run", event.get("count"))
            payload = build_alert_payload(
                body=body,
                title=title,
                event_id=_optional_event_id(event.get("id")),
                running_count=int(running_count) if running_count is not None else None,
            )
            await self.sink(payload)
            log.info("Queued local alert: %s", body[:80])
            return {"ok": True, "queued": "alert"}
        if event_type == "task_complete":
            return await self._dispatch_task_complete(event)
        if event_type == "screen":
            on = bool(event.get("on"))
            reason = str(event.get("reason") or "manual")
            payload = build_screen_control_payload(on, reason=reason)
            await self.sink(payload)
            log.info("Queued screen control on=%s reason=%s", on, reason)
            return {"ok": True, "queued": "screen", "on": on}
        if event_type in ("task_start", "task_finish", "activity"):
            return await self._dispatch_activity(event)
        if event_type == "usage":
            payload_obj = event.get("payload")
            if not isinstance(payload_obj, dict):
                raise ValueError("usage event requires a payload object")
            await self.sink(Payload("usage", payload_obj))
            log.info("Queued local usage payload")
            return {"ok": True, "queued": "usage"}
        raise ValueError(f"unknown event type: {event_type!r}")

    async def _dispatch_activity(self, event: dict[str, Any]) -> dict[str, Any]:
        event_type = event.get("type")
        if event_type == "task_start":
            if self.verify_task_starts:
                verification = self.transcripts.verify_user_turn_start(
                    event.get("turn_id"), event.get("transcript_path")
                )
                if not verification.verified:
                    log.info(
                        "Ignored unverified task start turn=%s session=%s reason=%s",
                        event.get("turn_id"),
                        event.get("session_id"),
                        verification.reason,
                    )
                    return {
                        "ok": True,
                        "queued": None,
                        "running": self.activity.count,
                        "ignored": "unverified_task_start",
                        "reason": verification.reason,
                    }
            changed = self.activity.start(event)
            self.transcripts.watch(event.get("turn_id"), event.get("transcript_path"))
        elif event_type == "task_finish":
            task = self.activity.finish_task(
                event,
                allow_oldest_fallback=_allow_oldest_fallback(event),
            )
            changed = task is not None
            self._unwatch_task(task)
        else:
            changed = self.activity.set_count(int(event.get("count") or event.get("run") or 0))
            self.transcripts.clear()

        if changed:
            payload = build_activity_payload(self.activity.count)
            await self.sink(payload)
            log.info("Queued activity count=%s", self.activity.count)
        return {
            "ok": True,
            "queued": "activity" if changed else None,
            "running": self.activity.count,
        }

    async def _dispatch_task_complete(self, event: dict[str, Any]) -> dict[str, Any]:
        task = self.activity.finish_task(
            event,
            allow_oldest_fallback=_allow_oldest_fallback(event),
        )
        activity_changed = task is not None
        self._unwatch_task(task)
        if activity_changed:
            await self.sink(build_activity_payload(self.activity.count))
            log.info("Queued activity count=%s", self.activity.count)

        body = str(event.get("body") or event.get("summary") or "")
        if bool(event.get("suppress_alert")):
            log.info("Suppressed task complete alert: %s", body[:80])
            return {
                "ok": True,
                "queued": "activity" if activity_changed else None,
                "running": self.activity.count,
            }

        title = str(event.get("title") or "任务完成！")
        payload = build_alert_payload(
            body=body,
            title=title,
            event_id=_optional_event_id(event.get("id")),
            running_count=self.activity.count,
        )
        await self.sink(payload)
        log.info("Queued local alert: %s", body[:80])
        return {
            "ok": True,
            "queued": "task_complete",
            "running": self.activity.count,
        }

    async def _finish_transcript_turn(self, turn_id: str, event_type: str) -> None:
        task = self.activity.finish_task(
            {"turn_id": turn_id}, allow_oldest_fallback=False
        )
        if task is None:
            return
        await self.sink(build_activity_payload(self.activity.count))
        log.info(
            "Queued activity count=%s after transcript event=%s",
            self.activity.count,
            event_type,
        )

    async def _activity_ttl_loop(self, stop_event: asyncio.Event) -> None:
        while not stop_event.is_set():
            try:
                await self._expire_stale_tasks()
            except asyncio.CancelledError:
                raise
            except Exception:
                log.exception("Activity TTL sweep failed")
            try:
                await asyncio.wait_for(
                    stop_event.wait(), timeout=self.activity_sweep_interval
                )
            except asyncio.TimeoutError:
                pass

    async def _expire_stale_tasks(self, now: float | None = None) -> int:
        expired = self.activity.expire_stale(self.activity_ttl, now=now)
        if not expired:
            return 0
        for task in expired:
            self._unwatch_task(task)
        await self.sink(build_activity_payload(self.activity.count))
        log.warning(
            "Expired %s stale activity task(s); running=%s ttl_sec=%s",
            len(expired),
            self.activity.count,
            self.activity_ttl,
        )
        return len(expired)

    def _activity_status(
        self,
        now: float | None = None,
        *,
        include_tasks: bool = False,
    ) -> dict[str, Any]:
        checked_at = time.monotonic() if now is None else now
        tasks = tuple(self.activity.active_tasks.values())
        oldest_age = (
            max(0.0, max(checked_at - task.started_at for task in tasks))
            if tasks
            else None
        )
        next_expiry = (
            max(
                0.0,
                min(
                    self.activity_ttl - (checked_at - task.last_seen_at)
                    for task in tasks
                ),
            )
            if tasks and self.activity_ttl > 0
            else None
        )
        status: dict[str, Any] = {
            "running": self.activity.count,
            "transcript_watches": self.transcripts.watch_count,
            "transcript_files": self.transcripts.file_count,
            "ttl_sec": self.activity_ttl,
            "oldest_age_sec": round(oldest_age, 1) if oldest_age is not None else None,
            "next_expiry_sec": round(next_expiry, 1) if next_expiry is not None else None,
        }
        if include_tasks:
            status["tasks"] = [
                {
                    "aliases": sorted(task.aliases),
                    "age_sec": round(max(0.0, checked_at - task.started_at), 1),
                    "idle_sec": round(max(0.0, checked_at - task.last_seen_at), 1),
                }
                for task in sorted(tasks, key=lambda item: item.started_at)
            ]
        return status

    def _unwatch_task(self, task: ActiveTask | None) -> None:
        if task is None:
            return
        prefix = "turn_id:"
        for alias in task.aliases:
            if alias.startswith(prefix):
                self.transcripts.unwatch(alias.removeprefix(prefix))


def _optional_event_id(value: object) -> str | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    return text[:31] or None


def _allow_oldest_fallback(event: dict[str, Any]) -> bool:
    return event.get("allow_oldest_fallback") is not False


def _identity_aliases(aliases: list[str]) -> list[str]:
    for field_name in IDENTITY_TASK_ID_FIELDS:
        prefix = f"{field_name}:"
        matches = [alias for alias in aliases if alias.startswith(prefix)]
        if matches:
            return matches
    return []


async def send_event(event: dict[str, Any], socket_path: Path = EVENT_SOCKET) -> dict[str, Any]:
    reader, writer = await asyncio.open_unix_connection(socket_path)
    writer.write(json.dumps(event, ensure_ascii=False, separators=(",", ":")).encode("utf-8"))
    await writer.drain()
    if writer.can_write_eof():
        writer.write_eof()
    raw = await asyncio.wait_for(reader.read(4096), timeout=3)
    writer.close()
    await writer.wait_closed()
    if not raw:
        return {}
    result = json.loads(raw.decode("utf-8"))
    return result if isinstance(result, dict) else {}
