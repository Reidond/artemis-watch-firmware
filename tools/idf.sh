#!/usr/bin/env bash
# Runs idf.py inside firmware/ with the pinned ESP-IDF environment.
#   tools/idf.sh build
#   tools/idf.sh menuconfig
set -euo pipefail

IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf-v5.5.6}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# shellcheck disable=SC1091
source "$IDF_PATH/export.sh" >/dev/null 2>&1
cd "$ROOT/firmware"
exec idf.py "$@"
