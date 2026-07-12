from pathlib import Path
import importlib.util
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
MODULE_PATH = ROOT / "tools" / "serial_device.py"
SPEC = importlib.util.spec_from_file_location("serial_device", MODULE_PATH)
serial_device = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
sys.modules["serial_device"] = serial_device
SPEC.loader.exec_module(serial_device)


def test_serial_identity_matches_device_selectors():
    identity = serial_device.SerialDeviceIdentity(
        port="/dev/cu.usbmodem211201",
        device_id="codexmeter-a3f91c",
        short_id="A3F91C",
        name="CodexMeter-A3F91C",
    )

    assert identity.matches("/dev/cu.usbmodem211201")
    assert identity.matches("cu.usbmodem211201")
    assert identity.matches("codexmeter-a3f91c")
    assert identity.matches("A3F91C")
    assert identity.matches("CodexMeter-A3F91C")
    assert not identity.matches("9B20D4")
