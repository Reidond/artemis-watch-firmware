# Artemis Control: watch ↔ companion app protocol

The watch is a BLE peripheral bonded with the iPhone (ANCS/AMS/CTS, see the
README). On top of that link it serves one custom GATT service for the
companion app. Every characteristic requires an encrypted link, which the
existing bond provides; the app never pairs on its own.

Service UUID: `9B2A0000-6E1D-4F3C-B8A5-2C7D1E0F4A93` (also in the scan
response, next to the name "Artemis").

| Characteristic | UUID | Properties | Payload |
|---|---|---|---|
| Info | `9B2A0001-…` | read, notify | JSON, watch → app |
| Settings | `9B2A0002-…` | read, write, notify | JSON |
| Weather | `9B2A0003-…` | write | JSON, app → watch |
| Command | `9B2A0004-…` | write | JSON, app → watch |
| OTA control | `9B2A0005-…` | write, notify | JSON |
| OTA data | `9B2A0006-…` | write without response | raw bytes |

`…` stands for the rest of the service UUID (`-6E1D-4F3C-B8A5-2C7D1E0F4A93`).
JSON payloads are UTF-8, at most 512 bytes (iOS uses long writes as needed).
Unknown keys are ignored in both directions, so either side can grow.

## Info (watch → app)

Read any time; notified on connect, whenever battery/charging changes, and
every 30 minutes. A notification wakes the app in the background, which is
how weather stays fresh without the app running.

```json
{"fw":"0.4.0","hw":3,"bat":82,"mv":3987,"chg":0,"t":1792500000,
 "wx":1792496400,"day":201}
```

| Key | Meaning |
|---|---|
| `fw` | firmware version |
| `hw` | hardware revision (efuse) |
| `bat` | battery percent, -1 when unknown (on USB since boot) |
| `mv` | battery millivolts |
| `chg` | 0 unplugged, 1 charging, 2 full |
| `t` | watch clock, unix seconds (UTC) |
| `wx` | when the watch last got weather (unix, 0 = never) |
| `day` | T+ mission day shown on the face |

## Settings (both ways)

Read returns every key. A write may carry any subset; the watch applies,
saves and notifies the full object back. Changes made on the watch are
notified the same way.

```json
{"lang":"uk","bright":60,"timeout":15,"h24":true,"flip":false,"sound":true,
 "tz":"EET-2EEST,M3.5.0/3,M10.5.0/4","epoch":1775082900}
```

| Key | Values |
|---|---|
| `lang` | `"uk"` or `"en"` |
| `bright` | 20, 40, 60, 80, 100 (other values snap to the nearest) |
| `timeout` | screen timeout in seconds: 5, 10, 15, 30, 60, 120 |
| `h24` | 24-hour clock |
| `flip` | screen turned 180° |
| `sound` | buzzer on |
| `tz` | POSIX TZ string; the app derives it from the iPhone's time zone |
| `epoch` | T+ counter start, unix seconds |

## Weather (app → watch)

```json
{"ts":1792496400,"place":"Київ","temp":12.4,"code":3,"hi":15.1,"lo":7.8,
 "hours":[[10,11.2,3],[11,12.0,2],[12,13.1,1],[13,14.0,1],[14,14.6,2],[15,14.2,3]]}
```

| Key | Meaning |
|---|---|
| `ts` | when the forecast was fetched (unix) |
| `place` | place name in the watch language |
| `temp` | current temperature, °C |
| `code` | WMO weather code (Open-Meteo `weather_code`) |
| `hi`, `lo` | today's max and min, °C |
| `hours` | next hours as `[local hour, °C, WMO code]` |

## Command (app → watch)

| Payload | Effect |
|---|---|
| `{"op":"find"}` | Ring and flash the screen for 10 s, even with sounds off |

## Firmware update (OTA)

The app writes the new app image into the spare OTA slot:

1. App → OTA control: `{"op":"begin","size":1191184,"sha256":"<64 hex>"}`.
   Watch erases the slot and answers `{"ok":true,"op":"begin"}` (or
   `{"ok":false,"err":"..."}`).
2. App → OTA data: the image in order, in chunks of up to MTU − 3 bytes,
   write without response. Flow control comes from CoreBluetooth
   (`canSendWriteWithoutResponse`); the link layer never drops packets.
   Watch notifies `{"op":"progress","recv":<bytes>}` every 32 KB.
3. App → OTA control: `{"op":"end"}`. Watch checks size and SHA-256, makes the
   new slot bootable, answers `{"ok":true,"op":"end"}` and restarts 1 s later.
4. `{"op":"abort"}` cancels at any point. A link drop also aborts; the old
   firmware keeps running.

The image is `firmware/build/artemis-fw.bin` (what `tools/flash.sh` writes at
0x20000). The stock bootloader boots whichever slot `otadata` selects.
