#!/usr/bin/env bash
# Runs tools/watchctl.py with ESP-IDF's Python (it ships pyserial).
#   tools/watch.sh log --reset
#   tools/watch.sh shot
set -euo pipefail

IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf-v5.5.6}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# shellcheck disable=SC1091
source "$IDF_PATH/export.sh" >/dev/null 2>&1
cd "$ROOT"
exec python "$ROOT/tools/watchctl.py" "$@"
