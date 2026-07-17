"""Quota providers."""

from __future__ import annotations

import asyncio
from dataclasses import dataclass, field, replace

from .app_server import AppServerError, JsonRpcClient
from .limits import (
    UsageSnapshot,
    usage_snapshot_from_rate_limits,
    usage_snapshot_with_token_activity,
)
from .local_usage import LocalTokenUsageReader


@dataclass(frozen=True)
class CodexUsageProvider:
    """Fetch Codex subscription limits through the local Codex App Server."""

    codex_bin: str = "codex"
    timeout_sec: float = 15.0
    refresh_token: bool = False
    local_usage_reader: LocalTokenUsageReader | None = field(
        default_factory=LocalTokenUsageReader,
        compare=False,
        repr=False,
    )

    async def fetch(self) -> UsageSnapshot:
        async with JsonRpcClient(self.codex_bin, self.timeout_sec) as client:
            await client.initialize()
            await client.request("account/read", {"refreshToken": self.refresh_token})
            rate_limits = await client.request("account/rateLimits/read")
            account_usage = None
            try:
                account_usage = await client.request("account/usage/read")
            except (AppServerError, asyncio.TimeoutError):
                # Token activity is optional. Preserve quota refreshes when the
                # account, Codex version, or backend does not expose it.
                pass

        local_today_tokens = None
        if self.local_usage_reader is not None:
            try:
                local_today_tokens = await asyncio.to_thread(
                    self.local_usage_reader.read_today_tokens
                )
            except Exception:
                # Local session events are also optional and must never block
                # the subscription quota refresh path.
                pass

        snapshot = usage_snapshot_from_rate_limits(rate_limits)
        if account_usage is not None:
            snapshot = usage_snapshot_with_token_activity(
                snapshot,
                account_usage,
                today_tokens_fallback=local_today_tokens,
            )
        elif local_today_tokens is not None:
            snapshot = replace(snapshot, today_tokens=local_today_tokens)
        return snapshot
