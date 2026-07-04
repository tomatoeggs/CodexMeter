#!/usr/bin/env python3
"""Small CLI demo for querying Codex ChatGPT subscription limits.

This talks to the local Codex App Server over stdio and prints the quota
windows reported by `account/rateLimits/read`. It does not read auth files or
print access tokens.
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import datetime as dt
import json
import shutil
import sys
from dataclasses import dataclass
from typing import Any


CLIENT_INFO = {
    "name": "codex-limits-demo",
    "title": "Codex Limits Demo",
    "version": "0.1.0",
}


class AppServerError(RuntimeError):
    pass


@dataclass
class JsonRpcClient:
    codex_bin: str
    timeout_sec: float

    proc: asyncio.subprocess.Process | None = None
    next_id: int = 1
    stderr_chunks: list[str] | None = None
    stderr_task: asyncio.Task[None] | None = None

    async def __aenter__(self) -> "JsonRpcClient":
        if shutil.which(self.codex_bin) is None:
            raise AppServerError(f"Cannot find `{self.codex_bin}` in PATH.")

        self.stderr_chunks = []
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
            if self.stderr_chunks is not None:
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
        await self._send({"method": method, **({"params": params} if params else {})})

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
                        f"{method} failed: {json.dumps(incoming['error'], ensure_ascii=False)}"
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
            stderr = "\n".join(self.stderr_chunks or [])
            suffix = f"\nApp server stderr:\n{stderr}" if stderr else ""
            raise AppServerError(f"Codex app-server exited before replying.{suffix}")
        try:
            return json.loads(line)
        except json.JSONDecodeError as exc:
            text = line.decode("utf-8", errors="replace").strip()
            raise AppServerError(f"Invalid JSON from app-server: {text}") from exc


def clamp_percent(value: float | None) -> float | None:
    if value is None:
        return None
    return min(100.0, max(0.0, value))


def format_percent(value: float | None) -> str:
    if value is None:
        return "--"
    if abs(value - round(value)) < 0.05:
        return f"{value:.0f}%"
    return f"{value:.1f}%"


def format_count(value: int | None) -> str:
    if value is None:
        return "--"
    return f"{value:,}"


def format_duration_mins(minutes: int | float | None) -> str:
    if minutes is None:
        return "unknown"
    total = max(0, int(round(minutes)))
    days, rem = divmod(total, 24 * 60)
    hours, mins = divmod(rem, 60)
    parts: list[str] = []
    if days:
        parts.append(f"{days}d")
    if hours:
        parts.append(f"{hours}h")
    if mins or not parts:
        parts.append(f"{mins}m")
    return " ".join(parts)


def format_window_label(minutes: int | None) -> str:
    if minutes == 300:
        return "5h"
    if minutes == 10080:
        return "7d"
    if minutes is None:
        return "unknown"
    return format_duration_mins(minutes)


def reset_info(resets_at: int | float | None) -> tuple[str, str | None, int | None]:
    if resets_at is None:
        return "unknown", None, None
    reset_dt = dt.datetime.fromtimestamp(float(resets_at)).astimezone()
    now = dt.datetime.now(reset_dt.tzinfo)
    delta_mins = max(0, int(round((reset_dt - now).total_seconds() / 60)))
    return (
        format_duration_mins(delta_mins),
        reset_dt.strftime("%Y-%m-%d %H:%M:%S %Z"),
        delta_mins,
    )


def normalize_limits(response: dict[str, Any], all_limits: bool = False) -> list[dict[str, Any]]:
    snapshots: list[dict[str, Any]] = []
    seen_limit_ids: set[str] = set()

    by_id = response.get("rateLimitsByLimitId")
    if isinstance(by_id, dict):
        candidates = by_id.values() if all_limits else [by_id.get("codex")]
        for snapshot in candidates:
            if isinstance(snapshot, dict):
                snapshots.append(snapshot)
                limit_id = snapshot.get("limitId") or snapshot.get("limitName")
                if isinstance(limit_id, str):
                    seen_limit_ids.add(limit_id)

    single = response.get("rateLimits")
    single_limit_id = (
        single.get("limitId") or single.get("limitName") if isinstance(single, dict) else None
    )
    if isinstance(single, dict) and (
        not snapshots or (all_limits and single_limit_id not in seen_limit_ids)
    ):
        snapshots.append(single)

    rows: list[dict[str, Any]] = []
    seen_rows: set[tuple[Any, ...]] = set()
    for snapshot in snapshots:
        limit_id = snapshot.get("limitId") or snapshot.get("limitName") or "unknown"
        status = snapshot.get("rateLimitReachedType") or "ok"
        plan_type = snapshot.get("planType")
        for window_name in ("primary", "secondary"):
            window = snapshot.get(window_name)
            if not isinstance(window, dict):
                continue
            duration = window.get("windowDurationMins")
            duration = int(duration) if isinstance(duration, (int, float)) else None
            used = clamp_percent(
                float(window["usedPercent"])
                if isinstance(window.get("usedPercent"), (int, float))
                else None
            )
            remaining = clamp_percent(100.0 - used) if used is not None else None
            reset_in, reset_at, reset_mins = reset_info(window.get("resetsAt"))
            row_key = (limit_id, window_name, duration, used, reset_at)
            if row_key in seen_rows:
                continue
            seen_rows.add(row_key)
            rows.append(
                {
                    "limit_id": limit_id,
                    "window": format_window_label(duration),
                    "window_duration_mins": duration,
                    "bucket": window_name,
                    "used_percent": used,
                    "remaining_percent": remaining,
                    "reset_in": reset_in,
                    "reset_at": reset_at,
                    "reset_mins": reset_mins,
                    "status": status,
                    "plan": plan_type,
                }
            )

    rows.sort(
        key=lambda row: (
            0 if row["limit_id"] == "codex" else 1,
            row["window_duration_mins"] or 10**9,
            row["limit_id"],
        )
    )
    return rows


def normalize_token_usage(response: dict[str, Any]) -> dict[str, Any]:
    buckets = response.get("dailyUsageBuckets")
    parsed: list[tuple[dt.date, int]] = []
    if isinstance(buckets, list):
        for bucket in buckets:
            if not isinstance(bucket, dict):
                continue
            start_date = bucket.get("startDate")
            tokens = bucket.get("tokens")
            if not isinstance(start_date, str) or not isinstance(tokens, int):
                continue
            with contextlib.suppress(ValueError):
                parsed.append((dt.date.fromisoformat(start_date), tokens))

    today = dt.date.today()

    def sum_since(days: int) -> int:
        start = today - dt.timedelta(days=days - 1)
        return sum(tokens for day, tokens in parsed if start <= day <= today)

    today_tokens = next((tokens for day, tokens in parsed if day == today), 0)
    summary = response.get("summary") if isinstance(response.get("summary"), dict) else {}
    return {
        "today_tokens": today_tokens,
        "last_7d_tokens": sum_since(7),
        "last_30d_tokens": sum_since(30),
        "lifetime_tokens": summary.get("lifetimeTokens"),
        "peak_daily_tokens": summary.get("peakDailyTokens"),
        "bucket_count": len(parsed),
    }


def build_summary(
    account_response: dict[str, Any],
    rate_limits_response: dict[str, Any],
    usage_response: dict[str, Any] | None,
    all_limits: bool,
) -> dict[str, Any]:
    account = account_response.get("account")
    account_type = account.get("type") if isinstance(account, dict) else None
    account_plan = account.get("planType") if isinstance(account, dict) else None
    result: dict[str, Any] = {
        "account": {
            "type": account_type,
            "plan": account_plan,
            "requires_openai_auth": account_response.get("requiresOpenaiAuth"),
        },
        "rate_limit_reset_credits": (
            rate_limits_response.get("rateLimitResetCredits") or {}
        ).get("availableCount"),
        "limits": normalize_limits(rate_limits_response, all_limits=all_limits),
    }
    if usage_response is not None:
        result["token_usage"] = normalize_token_usage(usage_response)
    return result


def print_table(summary: dict[str, Any]) -> None:
    account = summary["account"]
    account_type = account.get("type") or "unknown"
    plan = account.get("plan") or "unknown"
    print(f"Codex account: {account_type} (plan: {plan})")
    if account_type != "chatgpt":
        print("Warning: this demo is intended for ChatGPT subscription auth.")
    reset_credits = summary.get("rate_limit_reset_credits")
    if reset_credits is not None:
        print(f"Earned reset credits: {reset_credits}")
    print()

    rows = summary.get("limits") or []
    if not rows:
        print("No rate-limit windows returned.")
    else:
        headers = ["limit", "window", "used", "left", "reset in", "reset at", "status"]
        rendered = [
            [
                str(row["limit_id"]),
                str(row["window"]),
                format_percent(row["used_percent"]),
                format_percent(row["remaining_percent"]),
                str(row["reset_in"]),
                str(row["reset_at"] or "--"),
                str(row["status"]),
            ]
            for row in rows
        ]
        widths = [
            max(len(headers[i]), *(len(row[i]) for row in rendered))
            for i in range(len(headers))
        ]
        print("  ".join(headers[i].ljust(widths[i]) for i in range(len(headers))))
        print("  ".join("-" * widths[i] for i in range(len(headers))))
        for row in rendered:
            print("  ".join(row[i].ljust(widths[i]) for i in range(len(row))))

    token_usage = summary.get("token_usage")
    if isinstance(token_usage, dict):
        print()
        print("Token activity:")
        print(f"  today:    {format_count(token_usage.get('today_tokens'))}")
        print(f"  last 7d:  {format_count(token_usage.get('last_7d_tokens'))}")
        print(f"  last 30d: {format_count(token_usage.get('last_30d_tokens'))}")
        print(f"  lifetime: {format_count(token_usage.get('lifetime_tokens'))}")
        print(f"  buckets:  {format_count(token_usage.get('bucket_count'))}")


async def run(args: argparse.Namespace) -> int:
    async with JsonRpcClient(args.codex_bin, args.timeout) as client:
        await client.initialize()
        account_response = await client.request(
            "account/read", {"refreshToken": bool(args.refresh_token)}
        )
        rate_limits_response = await client.request("account/rateLimits/read")
        usage_response = None
        if args.tokens:
            usage_response = await client.request("account/usage/read")

    summary = build_summary(
        account_response,
        rate_limits_response,
        usage_response,
        all_limits=bool(args.all_limits),
    )
    if args.json:
        print(json.dumps(summary, indent=2, ensure_ascii=False))
    else:
        print_table(summary)
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Query Codex ChatGPT subscription usage limits via Codex App Server."
    )
    parser.add_argument(
        "--codex-bin",
        default="codex",
        help="Codex CLI executable to run. Defaults to `codex`.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=15.0,
        help="Timeout in seconds for each JSON-RPC request.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Print normalized JSON instead of a table.",
    )
    parser.add_argument(
        "--tokens",
        action="store_true",
        help="Also query account/usage/read and summarize token activity.",
    )
    parser.add_argument(
        "--all-limits",
        action="store_true",
        help="Show every returned limit bucket instead of only limit_id=codex.",
    )
    parser.add_argument(
        "--refresh-token",
        action="store_true",
        help="Ask account/read to proactively refresh ChatGPT auth.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return asyncio.run(run(args))
    except (AppServerError, asyncio.TimeoutError, BrokenPipeError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
