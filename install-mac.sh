#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
VENV="$ROOT/.venv"
PLIST="$HOME/Library/LaunchAgents/com.user.codexmeter.plist"
PY="$VENV/bin/python"
CODEX_BIN="${CODEX_BIN:-$(command -v codex || true)}"

if [[ -z "$CODEX_BIN" ]]; then
  echo "codex CLI not found. Set CODEX_BIN=/path/to/codex and rerun ./install-mac.sh" >&2
  exit 1
fi

python3 -m venv "$VENV"
"$PY" -m pip install --upgrade pip
"$PY" -m pip install -e "$ROOT"

mkdir -p "$HOME/Library/LaunchAgents" "$HOME/.codexmeter"

cat > "$PLIST" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>com.user.codexmeter</string>
  <key>ProgramArguments</key>
  <array>
    <string>$VENV/bin/codexmeterd</string>
    <string>--codex-bin</string>
    <string>$CODEX_BIN</string>
  </array>
  <key>EnvironmentVariables</key>
  <dict>
    <key>PATH</key>
    <string>$HOME/.local/bin:/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin</string>
  </dict>
  <key>WorkingDirectory</key>
  <string>$ROOT</string>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>StandardOutPath</key>
  <string>$HOME/.codexmeter/codexmeter.out.log</string>
  <key>StandardErrorPath</key>
  <string>$HOME/.codexmeter/codexmeter.err.log</string>
</dict>
</plist>
PLIST

HOOK_COMMAND="$("$PY" -c 'import shlex, sys; print(" ".join(shlex.quote(arg) for arg in sys.argv[1:]))' "$PY" "$ROOT/hooks/codexmeter_stop_hook.py")"
"$PY" "$ROOT/scripts/install_hook.py" --command "$HOOK_COMMAND"
"$PY" "$ROOT/scripts/install_notify.py" --python "$PY" --script "$ROOT/hooks/codexmeter_notify.py"

launchctl unload "$PLIST" >/dev/null 2>&1 || true
launchctl load -w "$PLIST"

echo "CodexMeter installed."
echo "Logs: tail -F ~/.codexmeter/codexmeter.out.log ~/.codexmeter/codexmeter.err.log ~/.codexmeter/codexmeter.log"
