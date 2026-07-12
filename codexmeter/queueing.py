"""Payload queue helpers shared by daemon and device workers."""

from __future__ import annotations

import asyncio

from .payloads import Payload

COALESCED_PAYLOAD_KINDS = {"usage", "activity", "control"}


async def put_latest(queue: "asyncio.Queue[Payload]", payload: Payload) -> None:
    if payload.kind in COALESCED_PAYLOAD_KINDS:
        kept: list[Payload] = []
        while True:
            try:
                existing = queue.get_nowait()
            except asyncio.QueueEmpty:
                break
            if existing.kind != payload.kind:
                kept.append(existing)
        for existing in kept:
            await queue.put(existing)

    if queue.full():
        await _drop_oldest_replaceable(queue)
    await queue.put(payload)


async def _drop_oldest_replaceable(queue: "asyncio.Queue[Payload]") -> None:
    kept: list[Payload] = []
    dropped = False
    while not queue.empty():
        existing = queue.get_nowait()
        if not dropped and existing.kind != "alert":
            dropped = True
            continue
        kept.append(existing)
    if not dropped and kept:
        kept.pop(0)
    for existing in kept:
        await queue.put(existing)
