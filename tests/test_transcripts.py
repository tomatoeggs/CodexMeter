import asyncio
import json

from codexmeter.transcripts import TranscriptWatcher


def _row(event_type, turn_id):
    return json.dumps(
        {
            "timestamp": "2026-07-17T10:00:00Z",
            "type": "event_msg",
            "payload": {"type": event_type, "turn_id": turn_id},
        },
        separators=(",", ":"),
    )


def _session_meta(thread_source="user", source="vscode"):
    return json.dumps(
        {
            "timestamp": "2026-07-17T10:00:00Z",
            "type": "session_meta",
            "payload": {"thread_source": thread_source, "source": source},
        },
        separators=(",", ":"),
    )


def test_watcher_ignores_history_and_detects_appended_turn_end(tmp_path):
    async def scenario():
        codex_home = tmp_path / ".codex"
        transcript = codex_home / "sessions" / "turn.jsonl"
        transcript.parent.mkdir(parents=True)
        transcript.write_text(_row("task_complete", "old") + "\n", encoding="utf-8")
        ended = []

        async def on_ended(turn_id, event_type):
            ended.append((turn_id, event_type))

        watcher = TranscriptWatcher(on_ended, codex_home=codex_home)
        assert watcher.watch("current", str(transcript)) is True
        await watcher.scan_once()
        assert ended == []

        with transcript.open("a", encoding="utf-8") as handle:
            handle.write(_row("turn_aborted", "current") + "\n")
        await watcher.scan_once()

        assert ended == [("current", "turn_aborted")]

    asyncio.run(scenario())


def test_watcher_reconciles_existing_end_for_watched_turn(tmp_path):
    async def scenario():
        codex_home = tmp_path / ".codex"
        transcript = codex_home / "sessions" / "turn.jsonl"
        transcript.parent.mkdir(parents=True)
        transcript.write_text(
            _row("task_complete", "turn-a") + "\n",
            encoding="utf-8",
        )
        ended = []

        async def on_ended(turn_id, event_type):
            ended.append((turn_id, event_type))

        watcher = TranscriptWatcher(on_ended, codex_home=codex_home)
        watcher.watch("turn-a", str(transcript))
        await watcher.scan_once()

        assert ended == [("turn-a", "task_complete")]

    asyncio.run(scenario())


def test_watcher_rewinds_when_turn_is_registered_late(tmp_path):
    async def scenario():
        codex_home = tmp_path / ".codex"
        transcript = codex_home / "sessions" / "thread.jsonl"
        transcript.parent.mkdir(parents=True)
        transcript.touch()
        ended = []

        async def on_ended(turn_id, event_type):
            ended.append((turn_id, event_type))

        watcher = TranscriptWatcher(on_ended, codex_home=codex_home)
        watcher.watch("turn-a", str(transcript))
        with transcript.open("a", encoding="utf-8") as handle:
            handle.write(_row("task_complete", "turn-b") + "\n")

        await watcher.scan_once()
        assert ended == []

        watcher.watch("turn-b", str(transcript))
        await watcher.scan_once()

        assert ended == [("turn-b", "task_complete")]

    asyncio.run(scenario())


def test_watcher_waits_for_complete_jsonl_row(tmp_path):
    async def scenario():
        codex_home = tmp_path / ".codex"
        transcript = codex_home / "sessions" / "turn.jsonl"
        transcript.parent.mkdir(parents=True)
        transcript.touch()
        ended = []

        async def on_ended(turn_id, event_type):
            ended.append((turn_id, event_type))

        watcher = TranscriptWatcher(on_ended, codex_home=codex_home)
        watcher.watch("turn-a", str(transcript))
        row = _row("turn_aborted", "turn-a")
        with transcript.open("a", encoding="utf-8") as handle:
            handle.write(row[: len(row) // 2])
        await watcher.scan_once()
        assert ended == []

        with transcript.open("a", encoding="utf-8") as handle:
            handle.write(row[len(row) // 2 :] + "\n")
        await watcher.scan_once()

        assert ended == [("turn-a", "turn_aborted")]

    asyncio.run(scenario())


def test_watcher_tracks_multiple_turns_in_one_transcript(tmp_path):
    async def scenario():
        codex_home = tmp_path / ".codex"
        transcript = codex_home / "sessions" / "thread.jsonl"
        transcript.parent.mkdir(parents=True)
        transcript.touch()
        ended = []

        async def on_ended(turn_id, event_type):
            ended.append((turn_id, event_type))

        watcher = TranscriptWatcher(on_ended, codex_home=codex_home)
        watcher.watch("turn-a", str(transcript))
        watcher.watch("turn-b", str(transcript))
        with transcript.open("a", encoding="utf-8") as handle:
            handle.write(_row("turn_aborted", "turn-a") + "\n")
            handle.write("{malformed}\n")
            handle.write(_row("task_complete", "turn-b") + "\n")

        await watcher.scan_once()

        assert ended == [
            ("turn-a", "turn_aborted"),
            ("turn-b", "task_complete"),
        ]

    asyncio.run(scenario())


def test_watcher_rejects_paths_outside_codex_home(tmp_path):
    async def on_ended(_turn_id, _event_type):
        raise AssertionError("untrusted transcript must not be read")

    codex_home = tmp_path / ".codex"
    codex_home.mkdir()
    outside = tmp_path / "outside.jsonl"
    outside.touch()
    watcher = TranscriptWatcher(on_ended, codex_home=codex_home)

    assert watcher.watch("turn-a", str(outside)) is False


def test_verifies_matching_user_turn_start(tmp_path):
    async def on_ended(_turn_id, _event_type):
        pass

    codex_home = tmp_path / ".codex"
    transcript = codex_home / "sessions" / "thread.jsonl"
    transcript.parent.mkdir(parents=True)
    transcript.write_text(
        _session_meta() + "\n" + _row("task_started", "turn-a") + "\n",
        encoding="utf-8",
    )
    watcher = TranscriptWatcher(on_ended, codex_home=codex_home)

    verification = watcher.verify_user_turn_start("turn-a", str(transcript))

    assert verification.verified is True
    assert verification.reason == "verified"


def test_rejects_internal_turn_start(tmp_path):
    async def on_ended(_turn_id, _event_type):
        pass

    codex_home = tmp_path / ".codex"
    transcript = codex_home / "sessions" / "guardian.jsonl"
    transcript.parent.mkdir(parents=True)
    transcript.write_text(
        _session_meta("subagent", {"subagent": {"other": "guardian"}})
        + "\n"
        + _row("task_started", "turn-a")
        + "\n",
        encoding="utf-8",
    )
    watcher = TranscriptWatcher(on_ended, codex_home=codex_home)

    verification = watcher.verify_user_turn_start("turn-a", str(transcript))

    assert verification.verified is False
    assert verification.reason == "internal_thread"


def test_rejects_unmatched_turn_start(tmp_path):
    async def on_ended(_turn_id, _event_type):
        pass

    codex_home = tmp_path / ".codex"
    transcript = codex_home / "sessions" / "thread.jsonl"
    transcript.parent.mkdir(parents=True)
    transcript.write_text(
        _session_meta() + "\n" + _row("task_started", "real-turn") + "\n",
        encoding="utf-8",
    )
    watcher = TranscriptWatcher(on_ended, codex_home=codex_home)

    verification = watcher.verify_user_turn_start("hook-turn", str(transcript))

    assert verification.verified is False
    assert verification.reason == "missing_task_started"


def test_rejects_already_ended_turn_start(tmp_path):
    async def on_ended(_turn_id, _event_type):
        pass

    codex_home = tmp_path / ".codex"
    transcript = codex_home / "sessions" / "thread.jsonl"
    transcript.parent.mkdir(parents=True)
    transcript.write_text(
        _session_meta()
        + "\n"
        + _row("task_started", "turn-a")
        + "\n"
        + _row("task_complete", "turn-a")
        + "\n",
        encoding="utf-8",
    )
    watcher = TranscriptWatcher(on_ended, codex_home=codex_home)

    verification = watcher.verify_user_turn_start("turn-a", str(transcript))

    assert verification.verified is False
    assert verification.reason == "turn_already_ended"


def test_watcher_reports_any_transcript_append_as_activity(tmp_path):
    async def scenario():
        codex_home = tmp_path / ".codex"
        transcript = codex_home / "sessions" / "turn.jsonl"
        transcript.parent.mkdir(parents=True)
        transcript.touch()
        observed = []

        async def on_ended(_turn_id, _event_type):
            pass

        watcher = TranscriptWatcher(
            on_ended,
            codex_home=codex_home,
            turns_active=lambda turn_ids: observed.append(turn_ids),
        )
        watcher.watch("turn-a", str(transcript))
        with transcript.open("a", encoding="utf-8") as handle:
            handle.write("{malformed}\n")

        await watcher.scan_once()

        assert observed == [{"turn-a"}]

    asyncio.run(scenario())
