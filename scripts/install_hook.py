#!/usr/bin/env python3
"""Merge CodexMeter hooks into a Codex hooks.json file."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import time
from pathlib import Path


def load_hooks(path: Path) -> dict:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as fh:
        data = json.load(fh)
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return data


def install_hook(
    path: Path,
    event_name: str,
    command: str,
    status_message: str,
) -> bool:
    root = load_hooks(path)
    events = root.setdefault("hooks", {})
    if not isinstance(events, dict):
        raise ValueError("hooks.hooks must be a JSON object")
    entries = events.setdefault(event_name, [])
    if not isinstance(entries, list):
        raise ValueError(f"hooks.hooks.{event_name} must be a list")

    hook = {
        "type": "command",
        "command": command,
        "timeout": 5,
        "statusMessage": status_message,
    }
    entry = {"matcher": "", "hooks": [hook]}

    for existing_entry in entries:
        if not isinstance(existing_entry, dict):
            continue
        for existing_hook in existing_entry.get("hooks", []):
            if isinstance(existing_hook, dict) and existing_hook.get("command") == command:
                return False

    entries.append(entry)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        backup = path.with_suffix(path.suffix + f".bak.{int(time.time())}")
        shutil.copy2(path, backup)
    tmp_path = path.with_suffix(path.suffix + ".tmp")
    with tmp_path.open("w", encoding="utf-8") as fh:
        json.dump(root, fh, ensure_ascii=False, indent=2)
        fh.write("\n")
    os.replace(tmp_path, path)
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hooks-json", default=str(Path.home() / ".codex" / "hooks.json"))
    parser.add_argument("--event", default="Stop")
    parser.add_argument("--command", required=True)
    parser.add_argument("--status-message", default="Updating CodexMeter")
    args = parser.parse_args()
    changed = install_hook(
        Path(args.hooks_json),
        args.event,
        args.command,
        args.status_message,
    )
    print("installed" if changed else "already-installed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
