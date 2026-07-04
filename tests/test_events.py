import asyncio

from codexmeter.events import EventServer


def test_activity_events_track_running_tasks():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink)
        result = await server._dispatch({"type": "task_start", "session_id": "a"})
        assert result["running"] == 1
        assert payloads[-1].kind == "activity"
        assert payloads[-1].data["run"] == 1

        result = await server._dispatch({"type": "task_start", "session_id": "b"})
        assert result["running"] == 2
        assert payloads[-1].data["run"] == 2

        result = await server._dispatch({"type": "task_finish", "session_id": "a"})
        assert result["running"] == 1
        assert payloads[-1].data["run"] == 1

        result = await server._dispatch({"type": "task_finish", "session_id": "b"})
        assert result["running"] == 0
        assert payloads[-1].data["run"] == 0

    asyncio.run(scenario())


def test_activity_finish_matches_any_shared_alias():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink)
        await server._dispatch({"type": "task_start", "turn_id": "turn-a"})
        result = await server._dispatch(
            {"type": "task_finish", "session_id": "session-a", "turn_id": "turn-a"}
        )
        assert result["running"] == 0
        assert payloads[-1].data["run"] == 0

    asyncio.run(scenario())


def test_activity_finish_falls_back_to_oldest_task_on_id_mismatch():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink)
        await server._dispatch({"type": "task_start", "session_id": "session-a"})
        result = await server._dispatch({"type": "task_finish", "session_id": "session-b"})
        assert result["running"] == 0
        assert payloads[-1].data["run"] == 0

    asyncio.run(scenario())


def test_task_complete_clears_activity_and_queues_alert():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink)
        await server._dispatch({"type": "task_start", "turn_id": "turn-a"})
        result = await server._dispatch(
            {
                "type": "task_complete",
                "session_id": "session-a",
                "turn_id": "turn-a",
                "title": "任务完成！",
                "body": "摘要",
            }
        )
        assert result["running"] == 0
        assert [payload.kind for payload in payloads[-2:]] == ["activity", "alert"]
        assert payloads[-2].data["run"] == 0
        assert payloads[-1].data["run"] == 0
        assert payloads[-1].data["body"] == "摘要"

    asyncio.run(scenario())


def test_task_complete_alert_carries_remaining_activity_count():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink)
        await server._dispatch({"type": "task_start", "session_id": "a"})
        await server._dispatch({"type": "task_start", "session_id": "b"})
        result = await server._dispatch(
            {
                "type": "task_complete",
                "session_id": "a",
                "title": "任务完成！",
                "body": "摘要",
            }
        )
        assert result["running"] == 1
        assert payloads[-2].kind == "activity"
        assert payloads[-2].data["run"] == 1
        assert payloads[-1].kind == "alert"
        assert payloads[-1].data["run"] == 1

    asyncio.run(scenario())
