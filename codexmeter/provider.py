"""Quota providers."""

from __future__ import annotations

from dataclasses import dataclass

from .app_server import JsonRpcClient
from .limits import UsageSnapshot, usage_snapshot_from_rate_limits


@dataclass(frozen=True)
class CodexUsageProvider:
    """Fetch Codex subscription limits through the local Codex App Server."""

    codex_bin: str = "codex"
    timeout_sec: float = 15.0
    refresh_token: bool = False

    async def fetch(self) -> UsageSnapshot:
        async with JsonRpcClient(self.codex_bin, self.timeout_sec) as client:
            await client.initialize()
            await client.request("account/read", {"refreshToken": self.refresh_token})
            rate_limits = await client.request("account/rateLimits/read")
        return usage_snapshot_from_rate_limits(rate_limits)
