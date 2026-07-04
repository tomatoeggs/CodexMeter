import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
NOTIFY = ROOT / "hooks" / "codexmeter_notify.py"


def test_notify_wrapper_succeeds_when_daemon_is_offline(tmp_path):
    marker = tmp_path / "called"
    proc = subprocess.run(
        [
            sys.executable,
            str(NOTIFY),
            "--",
            sys.executable,
            "-c",
            f"from pathlib import Path; Path({str(marker)!r}).write_text('ok')",
        ],
        input='{"last_assistant_message":"完成了实现。"}',
        text=True,
        capture_output=True,
        env={"CODEXMETER_SOCKET": str(tmp_path / "missing.sock")},
        timeout=3,
        check=False,
    )
    assert proc.returncode == 0
    assert marker.read_text() == "ok"
