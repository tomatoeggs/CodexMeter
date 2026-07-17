import asyncio
import json

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


def test_activity_finish_uses_unique_session_when_turn_id_changed():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink)
        await server._dispatch(
            {"type": "task_start", "session_id": "session-a", "turn_id": "turn-a"}
        )
        result = await server._dispatch(
            {
                "type": "task_finish",
                "session_id": "session-a",
                "turn_id": "turn-changed",
            }
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


def test_activity_tracks_distinct_turns_in_the_same_session():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink)
        await server._dispatch(
            {"type": "task_start", "session_id": "shared", "turn_id": "turn-a"}
        )
        result = await server._dispatch(
            {"type": "task_start", "session_id": "shared", "turn_id": "turn-b"}
        )

        assert result["running"] == 2
        assert payloads[-1].data["run"] == 2

    asyncio.run(scenario())


def test_turn_id_takes_priority_over_reused_task_id():
    async def scenario():
        async def sink(_payload):
            pass

        server = EventServer(sink)
        await server._dispatch(
            {"type": "task_start", "task_id": "shared", "turn_id": "turn-a"}
        )
        result = await server._dispatch(
            {"type": "task_start", "task_id": "shared", "turn_id": "turn-b"}
        )

        assert result["running"] == 2

    asyncio.run(scenario())


def test_activity_finish_does_not_guess_between_shared_session_turns():
    async def scenario():
        async def sink(_payload):
            pass

        server = EventServer(sink)
        await server._dispatch(
            {"type": "task_start", "session_id": "shared", "turn_id": "turn-a"}
        )
        await server._dispatch(
            {"type": "task_start", "session_id": "shared", "turn_id": "turn-b"}
        )
        result = await server._dispatch(
            {
                "type": "task_finish",
                "session_id": "shared",
                "turn_id": "turn-unknown",
            }
        )

        assert result["running"] == 2

    asyncio.run(scenario())


def test_transcript_finish_is_idempotent_with_later_stop(tmp_path):
    async def scenario():
        codex_home = tmp_path / ".codex"
        transcript = codex_home / "sessions" / "thread.jsonl"
        transcript.parent.mkdir(parents=True)
        transcript.touch()
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink, transcript_root=codex_home)
        await server._dispatch(
            {
                "type": "task_start",
                "session_id": "session-a",
                "turn_id": "turn-a",
                "transcript_path": str(transcript),
            }
        )
        await server._dispatch(
            {
                "type": "task_start",
                "session_id": "session-b",
                "turn_id": "turn-b",
            }
        )
        row = {
            "type": "event_msg",
            "payload": {"type": "turn_aborted", "turn_id": "turn-a"},
        }
        with transcript.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(row, separators=(",", ":")) + "\n")

        await server.transcripts.scan_once()
        assert server.activity.count == 1
        assert payloads[-1].kind == "activity"
        assert payloads[-1].data["run"] == 1

        result = await server._dispatch(
            {
                "type": "task_complete",
                "session_id": "session-a",
                "turn_id": "turn-a",
                "body": "done",
            }
        )

        assert result["running"] == 1
        assert payloads[-1].kind == "alert"
        assert payloads[-1].data["run"] == 1

    asyncio.run(scenario())


def test_activity_ttl_expires_only_stale_tasks_and_late_stop_is_safe():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink, activity_ttl=60)
        await server._dispatch(
            {"type": "task_start", "session_id": "session-a", "turn_id": "turn-a"}
        )
        await server._dispatch(
            {"type": "task_start", "session_id": "session-b", "turn_id": "turn-b"}
        )
        server.activity.active_tasks["turn_id:turn-a"].last_seen_at = 0
        server.activity.active_tasks["turn_id:turn-b"].last_seen_at = 30

        expired = await server._expire_stale_tasks(now=61)

        assert expired == 1
        assert server.activity.count == 1
        assert payloads[-1].kind == "activity"
        assert payloads[-1].data["run"] == 1

        result = await server._dispatch(
            {
                "type": "task_complete",
                "session_id": "session-a",
                "turn_id": "turn-a",
                "body": "done",
            }
        )
        assert result["running"] == 1
        assert payloads[-1].kind == "alert"
        assert payloads[-1].data["run"] == 1

    asyncio.run(scenario())


def test_activity_ttl_can_be_disabled():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink, activity_ttl=0)
        await server._dispatch({"type": "task_start", "turn_id": "turn-a"})
        server.activity.active_tasks["turn_id:turn-a"].last_seen_at = 0

        expired = await server._expire_stale_tasks(now=10_000)

        assert expired == 0
        assert server.activity.count == 1
        assert len(payloads) == 1

    asyncio.run(scenario())


def test_activity_ttl_loop_expires_task_after_real_wait():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(
            sink,
            activity_ttl=0.02,
            activity_sweep_interval=0.005,
        )
        await server._dispatch({"type": "task_start", "turn_id": "turn-a"})
        stop_event = asyncio.Event()
        loop_task = asyncio.create_task(server._activity_ttl_loop(stop_event))
        try:
            for _ in range(40):
                if server.activity.count == 0:
                    break
                await asyncio.sleep(0.005)
        finally:
            stop_event.set()
            await loop_task

        assert server.activity.count == 0
        assert payloads[-1].kind == "activity"
        assert payloads[-1].data["run"] == 0

    asyncio.run(scenario())


def test_transcript_append_renews_activity_lease(tmp_path):
    async def scenario():
        codex_home = tmp_path / ".codex"
        transcript = codex_home / "sessions" / "thread.jsonl"
        transcript.parent.mkdir(parents=True)
        transcript.touch()

        async def sink(_payload):
            pass

        server = EventServer(sink, transcript_root=codex_home, activity_ttl=60)
        await server._dispatch(
            {
                "type": "task_start",
                "turn_id": "turn-a",
                "transcript_path": str(transcript),
            }
        )
        task = server.activity.active_tasks["turn_id:turn-a"]
        task.last_seen_at = 0
        with transcript.open("a", encoding="utf-8") as handle:
            handle.write('{"type":"event_msg","payload":{"type":"token_count"}}\n')

        await server.transcripts.scan_once()
        renewed_at = task.last_seen_at

        assert renewed_at > 0
        assert await server._expire_stale_tasks(now=renewed_at + 59) == 0
        assert await server._expire_stale_tasks(now=renewed_at + 60) == 1

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


def test_alert_preserves_explicit_id_for_idempotent_retry():
    async def scenario():
        queued = []

        async def sink(payload):
            queued.append(payload)

        server = EventServer(sink)
        result = await server._dispatch(
            {"type": "alert", "id": "stable-alert-1", "body": "done"}
        )

        assert result["ok"] is True
        assert queued[0].data["id"] == "stable-alert-1"

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


def test_screen_event_queues_control_payload():
    async def scenario():
        payloads = []

        async def sink(payload):
            payloads.append(payload)

        server = EventServer(sink)
        result = await server._dispatch(
            {"type": "screen", "on": False, "reason": "manual"}
        )

        assert result == {"ok": True, "queued": "screen", "on": False}
        assert payloads[-1].kind == "control"
        assert payloads[-1].data["cmd"] == "screen"
        assert payloads[-1].data["on"] is False
        assert payloads[-1].data["why"] == "manual"

    asyncio.run(scenario())


def test_ping_includes_optional_status_provider_fields():
    async def scenario():
        async def sink(_payload):
            raise AssertionError("ping should not queue payloads")

        server = EventServer(sink, status_provider=lambda: {"ble": {"connected": True}})
        result = await server._dispatch({"type": "ping"})

        assert result == {
            "ok": True,
            "status": "running",
            "activity": {
                "running": 0,
                "transcript_watches": 0,
                "transcript_files": 0,
                "ttl_sec": 3600,
                "oldest_age_sec": None,
                "next_expiry_sec": None,
            },
            "ble": {"connected": True},
        }

    asyncio.run(scenario())


def test_activity_status_reports_age_and_next_expiry():
    async def scenario():
        async def sink(_payload):
            pass

        server = EventServer(sink, activity_ttl=60)
        await server._dispatch({"type": "task_start", "turn_id": "turn-a"})
        task = server.activity.active_tasks["turn_id:turn-a"]
        task.started_at = 10
        task.last_seen_at = 20

        status = server._activity_status(now=50)

        assert status["oldest_age_sec"] == 40
        assert status["next_expiry_sec"] == 30

    asyncio.run(scenario())
