import importlib.util
import json
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
