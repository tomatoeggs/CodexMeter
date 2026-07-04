"""Automatic screen on/off policy driven by macOS lock state."""

from __future__ import annotations

import asyncio
import logging
import re
import sys
import time
from collections.abc import Awaitable, Callable
from dataclasses import dataclass

from .ble import BleState
from .payloads import Payload, build_screen_control_payload
from .settings import AUTO_SCREEN_TIMEOUT_SEC, LOCK_POLL_INTERVAL_SEC

log = logging.getLogger(__name__)

PayloadSink = Callable[[Payload], Awaitable[None]]


@dataclass(frozen=True)
class ScreenPolicyResult:
    payloads: tuple[Payload, ...] = ()
    connect_control: Payload | None = None


@dataclass
class LockScreenPolicy:
    timeout_sec: float = AUTO_SCREEN_TIMEOUT_SEC
    locked: bool | None = None
    locked_since: float | None = None
    lock_off_sent: bool = False

    def observe(
        self,
        locked: bool,
        *,
        now: float,
        epoch_now: int | None = None,
    ) -> ScreenPolicyResult:
        payloads: list[Payload] = []

        if self.locked is None:
            self.locked = locked
            if locked:
                self.locked_since = now
                self.lock_off_sent = False
            else:
                payloads.append(
                    build_screen_control_payload(
                        True, reason="mac_unlocked", now=epoch_now
                    )
                )
        elif self.locked != locked:
            self.locked = locked
            self.lock_off_sent = False
            if locked:
                self.locked_since = now
            else:
                self.locked_since = None
                payloads.append(
                    build_screen_control_payload(
                        True, reason="mac_unlocked", now=epoch_now
                    )
                )

        if locked and self.locked_since is not None and not self.lock_off_sent:
            if now - self.locked_since >= self.timeout_sec:
                payloads.append(
                    build_screen_control_payload(
                        False, reason="mac_locked", now=epoch_now
                    )
                )
                self.lock_off_sent = True

        connect_control = None
        if not locked:
            connect_control = build_screen_control_payload(
                True, reason="ble_restored", now=epoch_now
            )

        return ScreenPolicyResult(tuple(payloads), connect_control)


class MacLockDetector:
    async def is_locked(self) -> bool | None:
        if sys.platform != "darwin":
            return None

        try:
            proc = await asyncio.create_subprocess_exec(
                "/usr/sbin/ioreg",
                "-n",
                "Root",
                "-d1",
                "-r",
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL,
            )
            stdout, _ = await asyncio.wait_for(proc.communicate(), timeout=2.0)
        except (OSError, asyncio.TimeoutError):
            log.debug("Failed to query macOS lock state", exc_info=True)
            return None

        if proc.returncode != 0:
            return None
        return parse_ioreg_lock_state(stdout.decode("utf-8", errors="replace"))


def parse_ioreg_lock_state(text: str) -> bool | None:
    match = re.search(r'"IOConsoleLocked"\s*=\s*(Yes|No)', text)
    if match:
        return match.group(1) == "Yes"

    match = re.search(r'"CGSSessionScreenIsLocked"\s*=\s*(Yes|No)', text)
    if match:
        return match.group(1) == "Yes"

    return None


async def screen_policy_loop(
    sink: PayloadSink,
    ble_state: BleState,
    stop_event: asyncio.Event,
    *,
    timeout_sec: float = AUTO_SCREEN_TIMEOUT_SEC,
    poll_interval_sec: float = LOCK_POLL_INTERVAL_SEC,
    detector: MacLockDetector | None = None,
) -> None:
    detector = detector or MacLockDetector()
    policy = LockScreenPolicy(timeout_sec=timeout_sec)

    while not stop_event.is_set():
        locked = await detector.is_locked()
        if locked is None:
            log.debug("macOS lock state unavailable; keeping previous screen policy")
        else:
            result = policy.observe(
                locked,
                now=time.monotonic(),
                epoch_now=int(time.time()),
            )
            ble_state.connect_control = result.connect_control
            for payload in result.payloads:
                await sink(payload)
                log.info(
                    "Queued screen control on=%s reason=%s",
                    payload.data.get("on"),
                    payload.data.get("why"),
                )

        try:
            await asyncio.wait_for(stop_event.wait(), timeout=poll_interval_sec)
        except asyncio.TimeoutError:
            pass
