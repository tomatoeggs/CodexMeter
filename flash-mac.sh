#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ENV_NAME="${1:-waveshare_amoled_216}"
PORT="${2:-}"
PIO_BIN="$ROOT/.venv/bin/pio"

if [[ ! -x "$PIO_BIN" ]]; then
  if command -v pio >/dev/null 2>&1; then
    PIO_BIN="$(command -v pio)"
  fi
fi

if [[ ! -x "$PIO_BIN" ]]; then
  echo "PlatformIO CLI not found. Install it first: python3 -m pip install platformio" >&2
  exit 1
fi

export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$ROOT/.platformio}"

if [[ -z "$PORT" ]]; then
  PORT="$(ls /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "$PORT" ]]; then
  echo "No serial port found. Pass one explicitly, e.g. ./flash-mac.sh $ENV_NAME /dev/cu.usbmodem1101" >&2
  exit 1
fi

"$PIO_BIN" run -d "$ROOT/firmware" -e "$ENV_NAME" -t upload --upload-port "$PORT"
