# NASA Artemis Watch 2.0 — hardware notes

Read from this unit on 2026-10-19 (esptool + efuse dump), cross-checked against
CircuitMess' open-source firmware: https://github.com/CircuitMess/GC_Artemis-Firmware

## Core

| Item | Value |
|---|---|
| SoC | ESP32-S3 (QFN56) rev v0.2, dual-core LX7 @ 240 MHz |
| Flash / PSRAM | 4 MB embedded (XMC) / 2 MB embedded (AP_3v3) |
| USB | Native USB-Serial/JTAG (VID 0x303A, PID 0x1001), shows up as `/dev/cu.usbmodem*` |
| Efuse BLOCK3 | PID `0x0003` (bits 16..31), HW revision `3` (bits 32..39) |

## Peripherals (HW revision 3)

| Part | Detail |
|---|---|
| Display | ST7735S color TFT, 128×128 visible (controller memory 128×160, offset 0,0), SPI @ 40 MHz, no CS |
| IMU | LSM6DS3TR-C, I²C `0x6A`, INT1/INT2 wired |
| RTC | PCF8563-compatible, I²C `0x51` (time registers from `0x02`) |
| Buttons | Up, Down, Select, Alt |
| LEDs | 6 single-color LEDs, 1 RGB LED (separate R/G/B pins) |
| Buzzer | PWM driven |
| Battery | ADC read with switched reference (BatteryV3 logic), charge-detect in/out pins |
| Radio | Wi-Fi 4 + BLE 5 |

## Pin map (revision 2 & 3)

| Function | GPIO | | Function | GPIO |
|---|---|---|---|---|
| BtnUp | 1 | | TftSck | 36 |
| BtnDown | 2 | | TftMosi | 35 |
| BtnSelect | 3 | | TftDc | 48 |
| BtnAlt | 21 | | TftRst | 34 |
| LedBl (backlight) | 33 | | Rgb R / G / B | 14 / 12 / 13 |
| Buzz | 47 | | ChrgIn / ChrgOut | 7 / 6 |
| BattRead | 5 | | Pwdn | 37 |
| BattVref | 4 | | Imu INT1 / INT2 | 38 / 39 |
| Usb detect | 42 | | Led 1..6 | 46, 45, 44, 43, 18, 17 |
| I²C SDA / SCL | 40 / 41 | | | |

## Flash layout (stock)

| Name | Type | Offset | Size |
|---|---|---|---|
| nvs | data/nvs | 0x009000 | 24K |
| phy_init | data/phy | 0x00F000 | 4K |
| factory | app | 0x010000 | 2968K |
| storage | data/spiffs | 0x2F6000 | 800K |
| coredump | data/coredump | 0x3BE000 | 256K |

Installed firmware: `Artemis-Firmware v2.0-2-g0656f15` (≈ release v2.1.1), built
2026-04-21 with ESP-IDF `v5.2-dev-1115-g3a45d4e949`. Upstream latest is v2.2.

## Stock software stack

- ESP-IDF 5.1 (commit 3a45d4e) + `ESP-IDF.patch` (FPU context-switch fix), Bluedroid BLE
- LVGL (UI) on LovyanGFX (display driver), JPEGDEC, SPIFFS image for assets
- iPhone: ANCS (notifications), AMS (media control), CTS (time sync) — no app needed
- Android: Gadgetbridge "Bangle.js" protocol over Nordic UART service
- Fonts are ASCII-only (glyphs 32..126): Cyrillic text renders as blanks

## Backup and restore

A full 4 MB dump of the stock flash is in `backup/stock-full-flash.bin`
(sha256 `ed5a68690c4189dbe4bcf926a9662b6196fba78b33321106ff98fe8c0ab6fe65`).
To restore it exactly:

```bash
source ~/esp/esp-idf-v5.5.6/export.sh
python -m esptool --chip esp32s3 -p /dev/cu.usbmodem2101 -b 921600 write_flash 0 backup/stock-full-flash.bin
```

## Brick hazards

- **GPIO37 (Pwdn) holds the power latch.** The stock bootloader hook warns that
  driving it low during boot bricks the device. Custom firmware keeps the stock
  bootloader at `0x0` and never drives GPIO37 low except to deliberately power off.
- The bootloader hook picks pins from efuse BLOCK3. Never burn efuses: they are
  one-time and the bootloader and pin map depend on them.
