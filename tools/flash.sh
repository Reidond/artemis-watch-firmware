#!/usr/bin/env bash
# Flashes partition table + OTA data + app. The stock bootloader at 0x0 is
# never written: it holds the power-latch safety hook (see docs/HARDWARE.md).
#   tools/flash.sh [port]
set -euo pipefail

IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf-v5.5.6}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/firmware/build}"
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"

if [[ -z "$PORT" ]]; then
	echo "No watch found on /dev/cu.usbmodem*. Plug it in and turn it on." >&2
	exit 1
fi

# shellcheck disable=SC1091
source "$IDF_PATH/export.sh" >/dev/null 2>&1

python -m esptool --chip esp32s3 --port "$PORT" --baud 921600 \
	--before default_reset --after hard_reset \
	write_flash --flash_mode keep --flash_freq keep --flash_size keep \
	0x8000 "$BUILD/partition_table/partition-table.bin" \
	0xF000 "$BUILD/ota_data_initial.bin" \
	0x20000 "$BUILD/artemis-fw.bin"
