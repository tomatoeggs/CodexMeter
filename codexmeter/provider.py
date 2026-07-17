"""Quota providers."""

from __future__ import annotations

import asyncio
import datetime as dt
import logging
from dataclasses import dataclass, field, replace
from typing import Any

from .app_server import AppServerError, JsonRpcClient
from .limits import (
    UsageSnapshot,
    usage_snapshot_from_rate_limits,
    usage_snapshot_with_token_activity,
)
from .local_usage import LocalTokenUsageReader


LOGGER = logging.getLogger(__name__)


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
    _usage_diagnostics: dict[str, object] = field(
        default_factory=dict,
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
            self._log_account_usage_change(
                account_usage,
                local_today_tokens,
                snapshot.today_tokens,
            )
        elif local_today_tokens is not None:
            snapshot = replace(snapshot, today_tokens=local_today_tokens)
        return snapshot

    def _log_account_usage_change(
        self,
        account_usage: dict[str, Any],
        local_today_tokens: int | None,
        selected_today_tokens: int | None,
    ) -> None:
        buckets = account_usage.get("dailyUsageBuckets")
        signature: tuple[tuple[str, int], ...] = ()
        if isinstance(buckets, list):
            valid_rows: list[tuple[str, int]] = []
            for bucket in buckets:
                if not isinstance(bucket, dict):
                    continue
                start_date = bucket.get("startDate")
                tokens = bucket.get("tokens")
                if not isinstance(start_date, str):
                    continue
                if isinstance(tokens, bool) or not isinstance(tokens, int):
                    continue
                valid_rows.append((start_date, max(0, tokens)))
            signature = tuple(sorted(valid_rows))

        observed_utc = dt.datetime.now(dt.timezone.utc)
        observed_local = observed_utc.astimezone()
        by_date = dict(signature)
        server_local_today = by_date.get(observed_local.date().isoformat())
        if server_local_today is None:
            selected_source = (
                "local_fallback" if selected_today_tokens is not None else "unavailable"
            )
        elif (
            local_today_tokens is not None
            and selected_today_tokens == local_today_tokens
            and selected_today_tokens != server_local_today
        ):
            selected_source = "local_mismatch"
        else:
            selected_source = "server"

        diagnostic_signature = (signature, selected_source)
        if self._usage_diagnostics.get("diagnostic_signature") == diagnostic_signature:
            return
        self._usage_diagnostics["diagnostic_signature"] = diagnostic_signature

        difference = (
            abs(server_local_today - local_today_tokens)
            if server_local_today is not None and local_today_tokens is not None
            else None
        )
        relative_difference = (
            difference / max(1, server_local_today, local_today_tokens)
            if difference is not None
            and server_local_today is not None
            and local_today_tokens is not None
            else None
        )
        recent = ",".join(
            f"{start_date}:{tokens}" for start_date, tokens in signature[-3:]
        ) or "none"
        log = LOGGER.warning if selected_source == "local_mismatch" else LOGGER.info
        log(
            "Codex daily usage diagnostic observed_utc=%s observed_local=%s "
            "utc_date=%s local_date=%s local_zone=%s recent=%s "
            "server_utc_today=%s server_local_today=%s local_today=%s "
            "difference=%s relative_difference=%s selected=%s",
            observed_utc.isoformat(timespec="seconds"),
            observed_local.isoformat(timespec="seconds"),
            observed_utc.date().isoformat(),
            observed_local.date().isoformat(),
            observed_local.tzname() or "unknown",
            recent,
            by_date.get(observed_utc.date().isoformat()),
            server_local_today,
            local_today_tokens,
            difference,
            f"{relative_difference:.3f}" if relative_difference is not None else None,
            selected_source,
        )
