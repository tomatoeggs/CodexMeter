"""Local Unix-socket event server used by Codex hooks and ctl commands."""

from __future__ import annotations

import asyncio
import json
import logging
from collections.abc import Awaitable, Callable
from pathlib import Path
from typing import Any

from .payloads import Payload, build_alert_payload
from .settings import EVENT_SOCKET

log = logging.getLogger(__name__)

PayloadSink = Callable[[Payload], Awaitable[None]]


class EventServer:
    def __init__(self, sink: PayloadSink, socket_path: Path = EVENT_SOCKET) -> None:
        self.sink = sink
        self.socket_path = socket_path
        self.server: asyncio.AbstractServer | None = None

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
            payload = build_alert_payload(body=body, title=title)
            await self.sink(payload)
            log.info("Queued local alert: %s", body[:80])
            return {"ok": True, "queued": "alert"}
        if event_type == "usage":
            payload_obj = event.get("payload")
            if not isinstance(payload_obj, dict):
                raise ValueError("usage event requires a payload object")
            await self.sink(Payload("usage", payload_obj))
            log.info("Queued local usage payload")
            return {"ok": True, "queued": "usage"}
        raise ValueError(f"unknown event type: {event_type!r}")


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
