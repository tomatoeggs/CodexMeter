import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STOP_HOOK = ROOT / "hooks" / "codexmeter_stop_hook.py"
START_HOOK = ROOT / "hooks" / "codexmeter_start_hook.py"


def test_stop_hook_succeeds_when_daemon_is_offline(tmp_path):
    missing_socket = tmp_path / "missing.sock"
    proc = subprocess.run(
        [sys.executable, str(STOP_HOOK)],
        input=json.dumps(
            {
                "hook_event_name": "Stop",
                "last_assistant_message": "完成了实现。\n\n测试也通过了。",
            }
        ),
        text=True,
        capture_output=True,
        env={"CODEXMETER_SOCKET": str(missing_socket)},
        timeout=2,
        check=False,
    )
    assert proc.returncode == 0
    assert json.loads(proc.stdout) == {"continue": True}


def test_start_hook_succeeds_when_daemon_is_offline(tmp_path):
    missing_socket = tmp_path / "missing.sock"
    proc = subprocess.run(
        [sys.executable, str(START_HOOK)],
        input=json.dumps(
            {
                "hook_event_name": "UserPromptSubmit",
                "session_id": "session-a",
                "cwd": "/tmp/project",
            }
        ),
        text=True,
        capture_output=True,
        env={"CODEXMETER_SOCKET": str(missing_socket)},
        timeout=2,
        check=False,
    )
    assert proc.returncode == 0
    assert json.loads(proc.stdout) == {"continue": True}


def test_start_hook_forwards_transcript_path(tmp_path):
    spec = importlib.util.spec_from_file_location("codexmeter_start_hook", START_HOOK)
    assert spec is not None and spec.loader is not None
    hook = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(hook)
    received = []
    hook.send_event = received.append
    transcript_path = str(tmp_path / "turn.jsonl")
    hook.send_task_start(
        {
            "hook_event_name": "UserPromptSubmit",
            "session_id": "session-a",
            "turn_id": "turn-a",
            "transcript_path": transcript_path,
        }
    )

    assert received[0]["transcript_path"] == transcript_path


def test_start_hook_skips_subagent_transcript(tmp_path):
    codex_home = tmp_path / ".codex"
    transcript = codex_home / "sessions" / "guardian.jsonl"
    transcript.parent.mkdir(parents=True)
    transcript.write_text(
        json.dumps(
            {
                "type": "session_meta",
                "payload": {
                    "thread_source": "subagent",
                    "source": {"subagent": {"other": "guardian"}},
                },
            },
            separators=(",", ":"),
        )
        + "\n",
        encoding="utf-8",
    )
    previous_codex_home = os.environ.get("CODEX_HOME")
    os.environ["CODEX_HOME"] = str(codex_home)

    spec = importlib.util.spec_from_file_location("codexmeter_start_hook", START_HOOK)
    assert spec is not None and spec.loader is not None
    hook = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(hook)
        received = []
        hook.send_event = received.append

        hook.send_task_start(
            {
                "hook_event_name": "UserPromptSubmit",
                "session_id": "session-a",
                "turn_id": "turn-a",
                "transcript_path": str(transcript),
            }
        )

        assert received == []
    finally:
        if previous_codex_home is None:
            os.environ.pop("CODEX_HOME", None)
        else:
            os.environ["CODEX_HOME"] = previous_codex_home


def test_start_hook_tracks_user_transcript(tmp_path):
    codex_home = tmp_path / ".codex"
    transcript = codex_home / "sessions" / "thread.jsonl"
    transcript.parent.mkdir(parents=True)
    transcript.write_text(
        json.dumps(
            {
                "type": "session_meta",
                "payload": {"thread_source": "user", "source": "vscode"},
            },
            separators=(",", ":"),
        )
        + "\n",
        encoding="utf-8",
    )
    previous_codex_home = os.environ.get("CODEX_HOME")
    os.environ["CODEX_HOME"] = str(codex_home)

    spec = importlib.util.spec_from_file_location("codexmeter_start_hook", START_HOOK)
    assert spec is not None and spec.loader is not None
    hook = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(hook)
        received = []
        hook.send_event = received.append

        hook.send_task_start(
            {
                "hook_event_name": "UserPromptSubmit",
                "session_id": "session-a",
                "turn_id": "turn-a",
                "transcript_path": str(transcript),
            }
        )

        assert received[0]["type"] == "task_start"
        assert received[0]["turn_id"] == "turn-a"
    finally:
        if previous_codex_home is None:
            os.environ.pop("CODEX_HOME", None)
        else:
            os.environ["CODEX_HOME"] = previous_codex_home


def test_stop_hook_suppresses_thread_title_descriptor():
    spec = importlib.util.spec_from_file_location("codexmeter_stop_hook", STOP_HOOK)
    assert spec is not None and spec.loader is not None
    hook = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(hook)
    received = []
    hook.send_event = received.append

    hook.notify_stop(
        {
            "hook_event_name": "Stop",
            "session_id": "session-title",
            "turn_id": "turn-title",
            "last_assistant_message": json.dumps(
                {
                    "title": "分析 skill 主流程",
                    "description": "梳理 skill 的核心流程、阶段与关键步骤",
                },
                ensure_ascii=False,
                separators=(",", ":"),
            ),
        }
    )

    assert received == [
        {
            "type": "task_finish",
            "source": "codex",
            "session_id": "session-title",
            "conversation_id": None,
            "turn_id": "turn-title",
            "task_id": None,
            "cwd": None,
            "allow_oldest_fallback": False,
        }
    ]


def test_stop_hook_suppresses_internal_transcript_alert(tmp_path):
    codex_home = tmp_path / ".codex"
    transcript = codex_home / "sessions" / "guardian.jsonl"
    transcript.parent.mkdir(parents=True)
    transcript.write_text(
        json.dumps(
            {
                "type": "session_meta",
                "payload": {
                    "thread_source": "subagent",
                    "source": {"subagent": {"other": "guardian"}},
                },
            },
            separators=(",", ":"),
        )
        + "\n",
        encoding="utf-8",
    )
    previous_codex_home = os.environ.get("CODEX_HOME")
    os.environ["CODEX_HOME"] = str(codex_home)

    spec = importlib.util.spec_from_file_location("codexmeter_stop_hook", STOP_HOOK)
    assert spec is not None and spec.loader is not None
    hook = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(hook)
        received = []
        hook.send_event = received.append

        hook.notify_stop(
            {
                "hook_event_name": "Stop",
                "session_id": "session-a",
                "turn_id": "turn-a",
                "transcript_path": str(transcript),
                "last_assistant_message": '{"outcome":"allow"}',
            }
        )

        assert received == [
            {
                "type": "task_finish",
                "source": "codex",
                "session_id": "session-a",
                "conversation_id": None,
                "turn_id": "turn-a",
                "task_id": None,
                "cwd": None,
                "allow_oldest_fallback": False,
            }
        ]
    finally:
        if previous_codex_home is None:
            os.environ.pop("CODEX_HOME", None)
        else:
            os.environ["CODEX_HOME"] = previous_codex_home


def test_stop_hook_keeps_regular_completion_alerts():
    spec = importlib.util.spec_from_file_location("codexmeter_stop_hook", STOP_HOOK)
    assert spec is not None and spec.loader is not None
    hook = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(hook)
    received = []
    hook.send_event = received.append

    hook.notify_stop(
        {
            "hook_event_name": "Stop",
            "session_id": "session-a",
            "turn_id": "turn-a",
            "last_assistant_message": "已完成实现。\n\n测试也通过了。",
        }
    )

    assert received[0]["type"] == "task_complete"
    assert received[0]["title"] == "任务完成！"
    assert received[0]["body"] == "已完成实现。 测试也通过了。"
