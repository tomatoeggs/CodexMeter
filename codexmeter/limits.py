"""Codex rate-limit normalization."""

from __future__ import annotations

import datetime as dt
from dataclasses import dataclass, replace
from typing import Any


WINDOW_5H_MINS = 300
WINDOW_7D_MINS = 7 * 24 * 60
USAGE_RECOVERY_CONFIRMATIONS = 2
USAGE_REMAINING_JUMP_THRESHOLD = 25
USAGE_RESET_GRACE_SEC = 60
USAGE_SAME_RESET_TOLERANCE_SEC = 120
USAGE_5H_RESET_JUMP_SEC = 60 * 60
USAGE_7D_RESET_JUMP_SEC = 24 * 60 * 60
USAGE_INITIAL_EMPTY_D7_WINDOW_SEC = 6 * 24 * 60 * 60

BEIJING_TZ = dt.timezone(dt.timedelta(hours=8), "CST")


@dataclass(frozen=True)
class LimitWindow:
    name: str
    duration_mins: int | None
    used_percent: float | None
    remaining_percent: float | None
    resets_at: int | None
    status: str


@dataclass(frozen=True)
class UsageSnapshot:
    source: str
    h5_remaining_percent: int | None
    h5_resets_at: int | None
    d7_remaining_percent: int | None
    d7_resets_at: int | None
    status: str
    generated_at: int
    today_tokens: int | None = None
    last_7d_tokens: int | None = None


@dataclass(frozen=True)
class TokenActivity:
    today_tokens: int | None
    last_7d_tokens: int | None


@dataclass(frozen=True)
class UsageSnapshotDecision:
    snapshot: UsageSnapshot
    accepted: bool
    reason: str


class UsageSnapshotStabilizer:
    """Filter transient App Server quota snapshots before they reach BLE."""

    def __init__(
        self,
        trusted: UsageSnapshot | None = None,
        confirmations: int = USAGE_RECOVERY_CONFIRMATIONS,
    ) -> None:
        self.confirmations = max(1, confirmations)
        self.trusted = trusted
        self.pending: UsageSnapshot | None = None
        self.pending_count = 0

    def stabilize(self, snapshot: UsageSnapshot) -> UsageSnapshotDecision:
        if self.trusted is None:
            self._accept(snapshot)
            return UsageSnapshotDecision(snapshot, accepted=True, reason="initial")

        reason = self._transient_reason(self.trusted, snapshot)
        if reason is None:
            self._accept(snapshot)
            return UsageSnapshotDecision(snapshot, accepted=True, reason="accepted")

        if self.pending is not None and _same_snapshot_values(self.pending, snapshot):
            self.pending_count += 1
        else:
            self.pending = snapshot
            self.pending_count = 1

        if _can_confirm_transient(reason) and self.pending_count >= self.confirmations:
            self._accept(snapshot)
            return UsageSnapshotDecision(
                snapshot,
                accepted=True,
                reason=f"confirmed:{reason}",
            )

        assert self.trusted is not None
        trusted = replace(self.trusted, generated_at=snapshot.generated_at)
        return UsageSnapshotDecision(trusted, accepted=False, reason=reason)

    def _accept(self, snapshot: UsageSnapshot) -> None:
        self.trusted = snapshot
        self.pending = None
        self.pending_count = 0

    def _transient_reason(
        self,
        trusted: UsageSnapshot,
        snapshot: UsageSnapshot,
    ) -> str | None:
        if trusted.status != "ok" or snapshot.status != "ok":
            return None

        h5_reason = _window_transient_reason(
            "h5",
            trusted.h5_remaining_percent,
            trusted.h5_resets_at,
            snapshot.h5_remaining_percent,
            snapshot.h5_resets_at,
            snapshot.generated_at,
            USAGE_5H_RESET_JUMP_SEC,
        )
        d7_reason = _window_transient_reason(
            "d7",
            trusted.d7_remaining_percent,
            trusted.d7_resets_at,
            snapshot.d7_remaining_percent,
            snapshot.d7_resets_at,
            snapshot.generated_at,
            USAGE_7D_RESET_JUMP_SEC,
        )
        return d7_reason or h5_reason


def clamp_percent(value: float | None) -> float | None:
    if value is None:
        return None
    return min(100.0, max(0.0, value))


def _as_int(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, (int, float)):
        return int(value)
    return None


def _window_name(minutes: int | None) -> str:
    if minutes == WINDOW_5H_MINS:
        return "5h"
    if minutes == WINDOW_7D_MINS:
        return "7d"
    if minutes is None:
        return "unknown"
    return f"{minutes}m"


def _candidate_snapshots(
    response: dict[str, Any], all_limits: bool = False
) -> list[dict[str, Any]]:
    snapshots: list[dict[str, Any]] = []
    seen: set[str] = set()

    by_id = response.get("rateLimitsByLimitId")
    if isinstance(by_id, dict):
        candidates = by_id.values() if all_limits else [by_id.get("codex")]
        for snapshot in candidates:
            if not isinstance(snapshot, dict):
                continue
            snapshots.append(snapshot)
            limit_id = snapshot.get("limitId") or snapshot.get("limitName")
            if isinstance(limit_id, str):
                seen.add(limit_id)

    single = response.get("rateLimits")
    single_limit_id = None
    if isinstance(single, dict):
        single_limit_id = single.get("limitId") or single.get("limitName")
    if isinstance(single, dict) and (
        not snapshots or (all_limits and single_limit_id not in seen)
    ):
        snapshots.append(single)

    return snapshots


def normalize_limits(response: dict[str, Any], all_limits: bool = False) -> list[LimitWindow]:
    windows: list[LimitWindow] = []
    seen_rows: set[tuple[Any, ...]] = set()

    for snapshot in _candidate_snapshots(response, all_limits=all_limits):
        status = snapshot.get("rateLimitReachedType") or "ok"
        status = status if isinstance(status, str) else "ok"
        for bucket in ("primary", "secondary"):
            window = snapshot.get(bucket)
            if not isinstance(window, dict):
                continue
            duration = _as_int(window.get("windowDurationMins"))
            used_raw = window.get("usedPercent")
            used = clamp_percent(float(used_raw)) if isinstance(used_raw, (int, float)) else None
            remaining = clamp_percent(100.0 - used) if used is not None else None
            resets_at = _as_int(window.get("resetsAt"))
            row_key = (bucket, duration, used, resets_at)
            if row_key in seen_rows:
                continue
            seen_rows.add(row_key)
            windows.append(
                LimitWindow(
                    name=_window_name(duration),
                    duration_mins=duration,
                    used_percent=used,
                    remaining_percent=remaining,
                    resets_at=resets_at,
                    status=status,
                )
            )

    windows.sort(key=lambda item: item.duration_mins or 10**9)
    return windows


def usage_snapshot_from_rate_limits(
    response: dict[str, Any], now: int | None = None
) -> UsageSnapshot:
    windows = normalize_limits(response)
    generated_at = int(now if now is not None else dt.datetime.now().timestamp())

    by_duration = {window.duration_mins: window for window in windows}
    h5 = by_duration.get(WINDOW_5H_MINS)
    d7 = by_duration.get(WINDOW_7D_MINS)

    # Older app-server responses may omit durations. Only infer those unknown
    # rows by position; never relabel an explicitly identified 7d window as 5h.
    unknown_duration = [window for window in windows if window.duration_mins is None]
    if h5 is None and unknown_duration:
        h5 = unknown_duration.pop(0)
    if d7 is None and unknown_duration:
        d7 = unknown_duration.pop(0)

    statuses = [window.status for window in (h5, d7) if window is not None]
    status = "ok"
    if any(item != "ok" for item in statuses):
        status = next(item for item in statuses if item != "ok")

    return UsageSnapshot(
        source="codex",
        h5_remaining_percent=_round_percent(h5.remaining_percent if h5 else None),
        h5_resets_at=h5.resets_at if h5 else None,
        d7_remaining_percent=_round_percent(d7.remaining_percent if d7 else None),
        d7_resets_at=d7.resets_at if d7 else None,
        status=status,
        generated_at=generated_at,
    )


def token_activity_from_account_usage(
    response: dict[str, Any],
    today: dt.date | None = None,
    today_tokens_fallback: int | None = None,
) -> TokenActivity:
    """Normalize account/usage/read daily buckets for the device summary."""

    buckets = response.get("dailyUsageBuckets")
    if not isinstance(buckets, list):
        return TokenActivity(
            today_tokens=today_tokens_fallback,
            last_7d_tokens=None,
        )

    by_date: dict[dt.date, int] = {}
    for bucket in buckets:
        if not isinstance(bucket, dict):
            continue
        start_date = bucket.get("startDate")
        tokens = bucket.get("tokens")
        if not isinstance(start_date, str) or isinstance(tokens, bool):
            continue
        if not isinstance(tokens, int):
            continue
        try:
            bucket_date = dt.date.fromisoformat(start_date)
        except ValueError:
            continue
        by_date[bucket_date] = max(0, tokens)

    current_date = today if today is not None else dt.date.today()
    first_date = current_date - dt.timedelta(days=6)
    has_current_bucket = current_date in by_date
    today_tokens = (
        by_date[current_date] if has_current_bucket else today_tokens_fallback
    )
    last_7d_tokens = sum(
        tokens for day, tokens in by_date.items() if first_date <= day <= current_date
    )
    if not has_current_bucket:
        if today_tokens_fallback is None:
            last_7d_tokens = None
        else:
            last_7d_tokens += today_tokens_fallback
    return TokenActivity(
        today_tokens=today_tokens,
        last_7d_tokens=last_7d_tokens,
    )


def usage_snapshot_with_token_activity(
    snapshot: UsageSnapshot,
    response: dict[str, Any],
    today: dt.date | None = None,
    today_tokens_fallback: int | None = None,
) -> UsageSnapshot:
    activity = token_activity_from_account_usage(
        response,
        today=today,
        today_tokens_fallback=today_tokens_fallback,
    )
    return replace(
        snapshot,
        today_tokens=activity.today_tokens,
        last_7d_tokens=activity.last_7d_tokens,
    )


def merge_missing_token_activity(
    snapshot: UsageSnapshot,
    fallback: UsageSnapshot | None,
) -> UsageSnapshot:
    """Keep the last token activity when its optional endpoint is unavailable."""

    if fallback is None:
        return snapshot
    return replace(
        snapshot,
        today_tokens=(
            snapshot.today_tokens
            if snapshot.today_tokens is not None
            else fallback.today_tokens
        ),
        last_7d_tokens=(
            snapshot.last_7d_tokens
            if snapshot.last_7d_tokens is not None
            else fallback.last_7d_tokens
        ),
    )


def _round_percent(value: float | None) -> int | None:
    if value is None:
        return None
    return int(round(clamp_percent(value) or 0))


def is_suspicious_initial_snapshot(snapshot: UsageSnapshot) -> bool:
    if snapshot.status != "ok":
        return False
    if snapshot.h5_remaining_percent is None or snapshot.d7_remaining_percent is None:
        return False
    if snapshot.h5_remaining_percent >= 100 or snapshot.d7_remaining_percent != 100:
        return False
    if snapshot.d7_resets_at is None:
        return False
    return (
        snapshot.d7_resets_at - snapshot.generated_at
        >= USAGE_INITIAL_EMPTY_D7_WINDOW_SEC
    )


def _same_snapshot_values(left: UsageSnapshot, right: UsageSnapshot) -> bool:
    return (
        left.h5_remaining_percent == right.h5_remaining_percent
        and left.h5_resets_at == right.h5_resets_at
        and left.d7_remaining_percent == right.d7_remaining_percent
        and left.d7_resets_at == right.d7_resets_at
        and left.status == right.status
    )


def _can_confirm_transient(reason: str) -> bool:
    return reason.endswith("_remaining_recovery")


def _window_transient_reason(
    name: str,
    trusted_remaining: int | None,
    trusted_reset: int | None,
    current_remaining: int | None,
    current_reset: int | None,
    now: int,
    reset_jump_threshold_sec: int,
) -> str | None:
    if (
        trusted_remaining is None
        or trusted_reset is None
        or current_remaining is None
        or current_reset is None
    ):
        return None
    if now >= trusted_reset - USAGE_RESET_GRACE_SEC:
        return None

    remaining_jump = current_remaining - trusted_remaining
    if remaining_jump < USAGE_REMAINING_JUMP_THRESHOLD:
        return None

    reset_jump = current_reset - trusted_reset
    if reset_jump >= reset_jump_threshold_sec:
        return f"{name}_early_reset_jump"

    if abs(reset_jump) <= USAGE_SAME_RESET_TOLERANCE_SEC:
        return f"{name}_remaining_recovery"

    return None


def format_beijing_time(epoch_seconds: int | None, with_date: bool) -> str:
    if epoch_seconds is None:
        return "--"
    value = dt.datetime.fromtimestamp(epoch_seconds, tz=BEIJING_TZ)
    if with_date:
        return f"{value.month}月{value.day}日 {value:%H:%M}"
    return value.strftime("%H:%M")
