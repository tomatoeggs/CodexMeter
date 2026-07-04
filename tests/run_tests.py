#!/usr/bin/env python3
"""Tiny test runner for environments without pytest."""

from __future__ import annotations

import importlib
import inspect
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

TEST_MODULES = [
    "tests.test_limits",
    "tests.test_payloads",
    "tests.test_events",
    "tests.test_hook",
    "tests.test_screenshot_tool",
    "tests.test_device_logs_tool",
]


def main() -> int:
    total = 0
    for module_name in TEST_MODULES:
        module = importlib.import_module(module_name)
        for name, func in inspect.getmembers(module, inspect.isfunction):
            if not name.startswith("test_"):
                continue
            total += 1
            params = inspect.signature(func).parameters
            if "tmp_path" in params:
                with tempfile.TemporaryDirectory() as tmp:
                    func(Path(tmp))
            else:
                func()
    print(f"{total} tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
