import asyncio

from codexmeter.supervision import supervise


def test_supervisor_propagates_critical_task_failure():
    async def scenario():
        stop_event = asyncio.Event()

        async def fail():
            raise RuntimeError("failed")

        async def wait():
            await stop_event.wait()

        try:
            await supervise(stop_event, {"fail": fail(), "wait": wait()})
        except RuntimeError as exc:
            assert str(exc) == "failed"
            return
        raise AssertionError("expected critical task failure")

    asyncio.run(scenario())


def test_supervisor_stops_services_cleanly():
    async def scenario():
        stop_event = asyncio.Event()
        stop_event.set()

        async def wait():
            await stop_event.wait()

        await supervise(stop_event, {"wait": wait()})

    asyncio.run(scenario())
