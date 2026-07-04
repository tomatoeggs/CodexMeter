"""Codex rate-limit normalization."""

from __future__ import annotations

import datetime as dt
from dataclasses import dataclass
from typing import Any


WINDOW_5H_MINS = 300
WINDOW_7D_MINS = 7 * 24 * 60

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

    # Older or partial app-server responses may omit durations. Codex exposes
    # primary then secondary in the same order as /status, so use that as a
    # conservative fallback.
    if h5 is None and windows:
        h5 = windows[0]
    if d7 is None and len(windows) > 1:
        d7 = windows[1]

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


def _round_percent(value: float | None) -> int | None:
    if value is None:
        return None
    return int(round(clamp_percent(value) or 0))


def format_beijing_time(epoch_seconds: int | None, with_date: bool) -> str:
    if epoch_seconds is None:
        return "--"
    value = dt.datetime.fromtimestamp(epoch_seconds, tz=BEIJING_TZ)
    if with_date:
        return f"{value.month}月{value.day}日 {value:%H:%M}"
    return value.strftime("%H:%M")
