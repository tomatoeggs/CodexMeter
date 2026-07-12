"""Fail-fast supervision for long-running daemon tasks."""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable, Mapping


async def supervise(
    stop_event: asyncio.Event,
    services: Mapping[str, Awaitable[None]],
) -> None:
    tasks = {
        asyncio.create_task(awaitable, name=name): name
        for name, awaitable in services.items()
    }
    stop_task = asyncio.create_task(stop_event.wait(), name="stop")
    try:
        done, _ = await asyncio.wait(
            {*tasks, stop_task}, return_when=asyncio.FIRST_COMPLETED
        )
        if stop_task in done:
            return

        failed = next(task for task in done if task is not stop_task)
        name = tasks[failed]
        error = failed.exception()
        if error is not None:
            raise error
        raise RuntimeError(f"critical task {name!r} exited unexpectedly")
    finally:
        stop_task.cancel()
        for task in tasks:
            task.cancel()
        await asyncio.gather(stop_task, *tasks, return_exceptions=True)
