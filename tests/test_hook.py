import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOOK = ROOT / "hooks" / "codexmeter_stop_hook.py"


def test_stop_hook_succeeds_when_daemon_is_offline(tmp_path):
    missing_socket = tmp_path / "missing.sock"
    proc = subprocess.run(
        [sys.executable, str(HOOK)],
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
