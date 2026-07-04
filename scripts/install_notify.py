#!/usr/bin/env python3
"""Install the CodexMeter notify wrapper into ~/.codex/config.toml."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import time
import tomllib
from pathlib import Path


NOTIFY_RE = re.compile(r"(?m)^notify\s*=\s*(.+)$")


def toml_array(items: list[str]) -> str:
    return "[" + ", ".join(json.dumps(item, ensure_ascii=False) for item in items) + "]"


def load_notify(path: Path) -> list[str]:
    if not path.exists():
        return []
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    notify = data.get("notify", [])
    return notify if isinstance(notify, list) and all(isinstance(i, str) for i in notify) else []


def install_notify(path: Path, wrapper: list[str]) -> bool:
    text = path.read_text(encoding="utf-8") if path.exists() else ""
    current = load_notify(path)
    script_path = wrapper[1] if len(wrapper) > 1 else ""
    if script_path and script_path in current:
        return False

    next_notify = [*wrapper]
    if current:
        next_notify.append("--")
        next_notify.extend(current)
    replacement = f"notify = {toml_array(next_notify)}"

    if NOTIFY_RE.search(text):
        new_text = NOTIFY_RE.sub(replacement, text, count=1)
    else:
        new_text = replacement + "\n" + text

    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        backup = path.with_suffix(path.suffix + f".bak.{int(time.time())}")
        shutil.copy2(path, backup)
    tmp_path = path.with_suffix(path.suffix + ".tmp")
    tmp_path.write_text(new_text, encoding="utf-8")
    os.replace(tmp_path, path)
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default=str(Path.home() / ".codex" / "config.toml"))
    parser.add_argument("--python", required=True)
    parser.add_argument("--script", required=True)
    args = parser.parse_args()
    changed = install_notify(Path(args.config), [args.python, args.script])
    print("installed" if changed else "already-installed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
