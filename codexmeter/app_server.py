"""Small JSON-RPC client for the local Codex App Server.

This module intentionally mirrors the already-validated flow in
``codex_limits_demo.py``: start ``codex app-server`` over stdio, initialize,
then make account requests. It does not read auth files directly.
"""

from __future__ import annotations

import asyncio
import contextlib
import json
import shutil
from dataclasses import dataclass, field
from typing import Any


CLIENT_INFO = {
    "name": "codexmeter",
    "title": "CodexMeter",
    "version": "0.1.0",
}


class AppServerError(RuntimeError):
    """Raised when Codex App Server cannot be queried."""


@dataclass
class JsonRpcClient:
    codex_bin: str = "codex"
    timeout_sec: float = 15.0

    proc: asyncio.subprocess.Process | None = None
    next_id: int = 1
    stderr_chunks: list[str] = field(default_factory=list)
    stderr_task: asyncio.Task[None] | None = None

    async def __aenter__(self) -> "JsonRpcClient":
        if shutil.which(self.codex_bin) is None:
            raise AppServerError(f"Cannot find `{self.codex_bin}` in PATH.")

        self.proc = await asyncio.create_subprocess_exec(
            self.codex_bin,
            "app-server",
            "--listen",
            "stdio://",
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        self.stderr_task = asyncio.create_task(self._read_stderr())
        return self

    async def __aexit__(self, *_exc: object) -> None:
        if self.stderr_task:
            self.stderr_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self.stderr_task

        if self.proc and self.proc.returncode is None:
            self.proc.terminate()
            with contextlib.suppress(asyncio.TimeoutError):
                await asyncio.wait_for(self.proc.wait(), timeout=2)
            if self.proc.returncode is None:
                self.proc.kill()
                await self.proc.wait()

    async def _read_stderr(self) -> None:
        assert self.proc and self.proc.stderr
        while True:
            chunk = await self.proc.stderr.readline()
            if not chunk:
                break
            text = chunk.decode("utf-8", errors="replace").rstrip()
            if text:
                self.stderr_chunks.append(text)
                del self.stderr_chunks[:-20]

    async def initialize(self) -> dict[str, Any]:
        result = await self.request(
            "initialize",
            {
                "clientInfo": CLIENT_INFO,
                "capabilities": {
                    "experimentalApi": True,
                    "requestAttestation": False,
                    "optOutNotificationMethods": [],
                },
            },
        )
        await self.notify("initialized")
        return result

    async def notify(self, method: str, params: dict[str, Any] | None = None) -> None:
        message: dict[str, Any] = {"method": method}
        if params is not None:
            message["params"] = params
        await self._send(message)

    async def request(
        self, method: str, params: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        request_id = self.next_id
        self.next_id += 1

        message: dict[str, Any] = {"method": method, "id": request_id}
        if params is not None:
            message["params"] = params
        await self._send(message)

        async def wait_for_response() -> dict[str, Any]:
            while True:
                incoming = await self._read_message()
                if incoming.get("id") != request_id:
                    continue
                if "error" in incoming:
                    raise AppServerError(
                        f"{method} failed: "
                        f"{json.dumps(incoming['error'], ensure_ascii=False)}"
                    )
                result = incoming.get("result")
                return result if isinstance(result, dict) else {}

        return await asyncio.wait_for(wait_for_response(), timeout=self.timeout_sec)

    async def _send(self, message: dict[str, Any]) -> None:
        assert self.proc and self.proc.stdin
        payload = json.dumps(message, separators=(",", ":")).encode("utf-8") + b"\n"
        self.proc.stdin.write(payload)
        await self.proc.stdin.drain()

    async def _read_message(self) -> dict[str, Any]:
        assert self.proc and self.proc.stdout
        line = await self.proc.stdout.readline()
        if not line:
            stderr = "\n".join(self.stderr_chunks)
            suffix = f"\nApp server stderr:\n{stderr}" if stderr else ""
            raise AppServerError(f"Codex app-server exited before replying.{suffix}")
        try:
            return json.loads(line)
        except json.JSONDecodeError as exc:
            text = line.decode("utf-8", errors="replace").strip()
            raise AppServerError(f"Invalid JSON from app-server: {text}") from exc
