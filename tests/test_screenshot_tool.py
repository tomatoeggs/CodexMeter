from pathlib import Path
import importlib.util
import struct


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "capture_screenshot.py"
SPEC = importlib.util.spec_from_file_location("capture_screenshot", MODULE_PATH)
capture_screenshot = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(capture_screenshot)


def _chunk_payload(png: bytes, kind: bytes) -> bytes:
    offset = len(capture_screenshot.PNG_SIGNATURE)
    while offset < len(png):
        size = struct.unpack(">I", png[offset : offset + 4])[0]
        chunk_kind = png[offset + 4 : offset + 8]
        payload = png[offset + 8 : offset + 8 + size]
        if chunk_kind == kind:
            return payload
        offset += 12 + size
    raise AssertionError(f"missing PNG chunk {kind!r}")


def test_rgb565le_to_png_encodes_png_header_and_dimensions():
    raw = bytes([0x00, 0xF8, 0xE0, 0x07])
    png = capture_screenshot.rgb565le_to_png(2, 1, raw)
    assert png.startswith(capture_screenshot.PNG_SIGNATURE)

    ihdr = _chunk_payload(png, b"IHDR")
    width, height, bit_depth, color_type = struct.unpack(">IIBB", ihdr[:10])
    assert (width, height) == (2, 1)
    assert bit_depth == 8
    assert color_type == 2


def test_rgb565le_to_png_rejects_wrong_size():
    try:
        capture_screenshot.rgb565le_to_png(2, 2, b"\x00\x00")
    except ValueError as exc:
        assert "does not match" in str(exc)
    else:
        raise AssertionError("expected ValueError")


def test_detect_port_scoring_prefers_firmware_cdc_port():
    ports = [
        "/dev/cu.usbmodem0000000000004",
        "/dev/cu.usbmodem211201",
        "/dev/cu.usbserial-test",
    ]
    assert sorted(ports, key=capture_screenshot._port_score)[0] == "/dev/cu.usbmodem211201"
