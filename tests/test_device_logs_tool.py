from pathlib import Path
import importlib.util
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
MODULE_PATH = ROOT / "tools" / "read_device_logs.py"
SPEC = importlib.util.spec_from_file_location("read_device_logs", MODULE_PATH)
read_device_logs = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(read_device_logs)


def test_build_logs_command():
    assert read_device_logs.build_logs_command() == "logs"
    assert read_device_logs.build_logs_command(12) == "logs 12"
