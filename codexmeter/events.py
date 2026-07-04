"""Local Unix-socket event server used by Codex hooks and ctl commands."""

from __future__ import annotations

import asyncio
import json
import logging
import time
from collections.abc import Awaitable, Callable
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .payloads import Payload, build_activity_payload, build_alert_payload
from .settings import EVENT_SOCKET

log = logging.getLogger(__name__)

PayloadSink = Callable[[Payload], Awaitable[None]]


TASK_ID_FIELDS = ("task_id", "turn_id", "session_id", "conversation_id")


@dataclass
class ActiveTask:
    aliases: set[str]
    started_at: float = field(default_factory=time.monotonic)


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
    anonymous_seq: int = 0
    last_changed_at: float = field(default_factory=time.monotonic)

    @property
    def count(self) -> int:
        return len(self.active_tasks)

    def start(self, event: dict[str, Any]) -> bool:
        before = self.count
        aliases = task_aliases(event)
        existing_key = self._find_task(aliases)
        if existing_key is not None:
            self.active_tasks[existing_key].aliases.update(aliases)
            return self._changed(before)

        primary = aliases[0] if aliases else self._anonymous_key()
        self.active_tasks[primary] = ActiveTask(set(aliases or [primary]))
        return self._changed(before)

    def finish(self, event: dict[str, Any]) -> bool:
        before = self.count
        aliases = task_aliases(event)
        matched_key = self._find_task(aliases)
        if matched_key is None and self.active_tasks:
            matched_key = self._oldest_task()
        if matched_key is not None:
            del self.active_tasks[matched_key]
        return self._changed(before)

    def set_count(self, count: int) -> bool:
        before = self.count
        self.active_tasks.clear()
        for _ in range(max(0, int(count))):
            key = self._anonymous_key()
            self.active_tasks[key] = ActiveTask({key})
        return self._changed(before)

    def _find_task(self, aliases: list[str]) -> str | None:
        if not aliases:
            return None
        alias_set = set(aliases)
        for key, task in self.active_tasks.items():
            if task.aliases & alias_set:
                return key
        return None

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
    def __init__(self, sink: PayloadSink, socket_path: Path = EVENT_SOCKET) -> None:
        self.sink = sink
        self.socket_path = socket_path
        self.server: asyncio.AbstractServer | None = None
        self.activity = ActivityTracker()

    async def run(self, stop_event: asyncio.Event) -> None:
        self.socket_path.parent.mkdir(parents=True, exist_ok=True)
        if self.socket_path.exists():
            self.socket_path.unlink()
        self.server = await asyncio.start_unix_server(self._handle_client, self.socket_path)
        log.info("Listening for Codex events on %s", self.socket_path)
        try:
            async with self.server:
                await stop_event.wait()
        finally:
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
            return {"ok": True, "status": "running"}
        if event_type == "alert":
            body = str(event.get("body") or event.get("summary") or "")
            title = str(event.get("title") or "任务完成！")
            running_count = event.get("run", event.get("count"))
            payload = build_alert_payload(
                body=body,
                title=title,
                running_count=int(running_count) if running_count is not None else None,
            )
            await self.sink(payload)
            log.info("Queued local alert: %s", body[:80])
            return {"ok": True, "queued": "alert"}
        if event_type == "task_complete":
            return await self._dispatch_task_complete(event)
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
            changed = self.activity.start(event)
        elif event_type == "task_finish":
            changed = self.activity.finish(event)
        else:
            changed = self.activity.set_count(int(event.get("count") or event.get("run") or 0))

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
        activity_changed = self.activity.finish(event)
        if activity_changed:
            await self.sink(build_activity_payload(self.activity.count))
            log.info("Queued activity count=%s", self.activity.count)

        body = str(event.get("body") or event.get("summary") or "")
        title = str(event.get("title") or "任务完成！")
        payload = build_alert_payload(
            body=body,
            title=title,
            running_count=self.activity.count,
        )
        await self.sink(payload)
        log.info("Queued local alert: %s", body[:80])
        return {
            "ok": True,
            "queued": "task_complete",
            "running": self.activity.count,
        }


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
