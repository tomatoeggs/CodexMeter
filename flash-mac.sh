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

find_ports() {
  shopt -s nullglob
  local ports=(/dev/cu.usbmodem* /dev/cu.usbserial*)
  local port
  for port in "${ports[@]}"; do
    if [[ "$port" != *000000000000* ]]; then
      echo "$port"
    fi
  done
  for port in "${ports[@]}"; do
    if [[ "$port" == *000000000000* ]]; then
      echo "$port"
    fi
  done
}

upload_one() {
  local port="$1"
  echo "Uploading LittleFS data partition to $port..."
  "$PIO_BIN" run -d "$ROOT/firmware" -e "$ENV_NAME" -t uploadfs --upload-port "$port"
  echo "Uploading firmware to $port..."
  "$PIO_BIN" run -d "$ROOT/firmware" -e "$ENV_NAME" -t upload --upload-port "$port"
}

if [[ "$PORT" == "--all" ]]; then
  PORTS=()
  while IFS= read -r port; do
    PORTS+=("$port")
  done < <(find_ports)
  if [[ "${#PORTS[@]}" -eq 0 ]]; then
    echo "No serial port found." >&2
    exit 1
  fi
  FAILED=0
  for port in "${PORTS[@]}"; do
    if ! upload_one "$port"; then
      echo "Upload failed on $port" >&2
      FAILED=1
    fi
  done
  exit "$FAILED"
fi

if [[ -z "$PORT" ]]; then
  PORTS=()
  while IFS= read -r port; do
    PORTS+=("$port")
  done < <(find_ports)
  if [[ "${#PORTS[@]}" -eq 1 ]]; then
    PORT="${PORTS[0]}"
  elif [[ "${#PORTS[@]}" -gt 1 ]]; then
    echo "Multiple serial ports found. Pass one explicitly or use --all:" >&2
    printf '  %s\n' "${PORTS[@]}" >&2
    exit 1
  fi
fi

if [[ -z "$PORT" ]]; then
  echo "No serial port found. Pass one explicitly, e.g. ./flash-mac.sh $ENV_NAME /dev/cu.usbmodem1101" >&2
  exit 1
fi

upload_one "$PORT"
