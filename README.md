# Artemis Watch custom firmware

Custom firmware for the CircuitMess NASA Artemis Watch 2.0 (ESP32-S3), with a
Ukrainian/English UI. It shows the iPhone's notifications and calls, controls
its music and takes the time from it, with nothing installed on the phone.
A companion iPhone app adds settings, weather and firmware updates over
Bluetooth. Hardware notes, pin map and brick hazards are in
[docs/HARDWARE.md](docs/HARDWARE.md); what comes next is in [docs/ROADMAP.md](docs/ROADMAP.md).

This is an independent hobby project, not affiliated with or endorsed by
CircuitMess or NASA. Flashing it replaces the watch's stock firmware; see
[Restore the stock firmware](#restore-the-stock-firmware) and back up first.

## Setup (once)

```bash
mkdir -p ~/esp && git clone -b v5.5.6 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v5.5.6
~/esp/esp-idf-v5.5.6/install.sh esp32s3
```

The scripts in `tools/` use `~/esp/esp-idf-v5.5.6` unless `IDF_PATH` is set.

Editor support (Zed, VS Code): clangd needs Espressif's LLVM build and the
repo's `tools/bin` on `PATH`, which `mise.toml` provides:

```bash
python ~/esp/esp-idf-v5.5.6/tools/idf_tools.py install esp-clang
mise trust
```

Fonts are generated at 1 bit per pixel for the dithered look. The checked-in
set is [Iosevka](https://github.com/be5invis/Iosevka) (SIL OFL 1.1); this is
only needed after changing fonts or character ranges: `tools/gen_fonts.sh`.
If you own PragmataPro, `tools/gen_fonts.sh pragmata` writes a git-ignored
set to `firmware/main/ui/fonts/local/` that the build uses instead (run
`tools/idf.sh reconfigure` afterwards). Both fonts are 0.5 em wide, so the
layouts fit either.

Art assets come from `python3 tools/gen_art.py`: the Earth land mask
(Natural Earth, public domain) and the sprites in `tools/sprites/*.txt`,
which are plain-text pixel art (one character per pixel). Generated files are
checked in too.

## Look

The UI follows concept art made with Codex image generation (the images stay
local in `design/concepts/`, not in git; its README describes them): four flat colours (black, off-white, NASA red,
grey) and Bayer-dithered patterns for every other shade (`ui/dither.h`).
The watch face is the Orbit face: a T+ mission-day counter (days since the
Artemis II launch; console `epoch <unix>` changes it), the time, Earth with
the real day/night line seen over Kyiv, the Moon in its real phase, and a red
craft that flies the Earth–Moon loop once a day (Earth at midnight, the Moon
at noon).

## Build and flash

```bash
tools/idf.sh build
tools/flash.sh
```

`flash.sh` writes the partition table, OTA data and app only. The stock
bootloader at `0x0` is never touched (it holds the power-latch safety hook).

## Pairing the iPhone

1. On the iPhone: Settings > Bluetooth, tap **Artemis** under Other Devices.
2. The watch shows a six-digit code. If the iPhone shows the same one, click
   the wheel on the watch and tap Pair on the iPhone.
3. Allow Artemis to receive notifications when iOS asks (later: Settings >
   Bluetooth > Artemis (i) > Share System Notifications).

The watch then reconnects by itself. It syncs the clock and time zone from the
phone on every connection. The Bluetooth icon on the watch face is blue when
the phone is connected.

The watch advertises with its own random address, so an iPhone that was paired
with the stock firmware ("Artemis Watch") sees a new device. If pairing fails,
the watch says so: forget Artemis on the iPhone (Bluetooth > Artemis (i) >
Forget This Device) and pair again. To drop the bond on the watch: Settings >
Phone, click the wheel.

## Companion app (iOS)

`ios/` holds a SwiftUI app that talks to the watch over the "Artemis Control"
BLE service ([docs/BLE_PROTOCOL.md](docs/BLE_PROTOCOL.md)). It:

- shows battery, firmware and the T+ day, and finds the watch (ring + flash)
- changes watch settings: language, brightness, screen timeout, 12/24 h,
  flip, sounds, the T+ start date; keeps the watch's time zone and DST rules
  in step with the iPhone
- sends weather (Open-Meteo, no account) for the iPhone's location, also in
  the background: the watch checks in every 30 minutes, which wakes the app
- updates the watch firmware over Bluetooth: the app ships the firmware from
  `firmware/build/` at build time, or takes a `.bin` from Files

The UI follows the iPhone's language (Ukrainian or English).

Build and install (the watch must already run firmware 0.4.0+, flashed once
over USB, and be paired with the iPhone):

```bash
brew install xcodegen
cp ios/Local.xcconfig.example ios/Local.xcconfig   # set DEVELOPMENT_TEAM, BUNDLE_ID_PREFIX
cd ios && xcodegen && open Artemis.xcodeproj
```

Then run the Artemis scheme on the iPhone (unlocked, Developer Mode on).
`tools/gen_app_icon.py` renders the icon from the watch-face art.

## Talking to the watch

`tools/watch.sh` drives the USB serial console:

| Command | What it does |
|---|---|
| `tools/watch.sh log --reset` | Reset and print the boot log |
| `tools/watch.sh shot` | Save a screenshot to `screenshots/` |
| `tools/watch.sh press select` | Inject buttons: `up`, `down`, `select`, `alt` (`--hold` for long); see [Buttons](#buttons) |
| `tools/watch.sh synctime` | Set the watch clock from this Mac |
| `tools/watch.sh cmd "lang en"` | Any console command: `info`, `tz`, `lang`, `epoch`, `wake`, `ble`, `art`, `reboot`, `help` |
| `tools/watch.sh cmd 'demo notif "Мама" "Привіт 😀"'` | Fake phone data to check screens: `demo help` |

## Buttons

The watch has two controls on the bottom edge, left of the screen: a wheel
you roll left or right and click, and the power button next to it. Every
screen shows what they do in a strip along its bottom edge, right next to
them: ← and → for the wheel, • for a click, ⏻ for the power button, each
followed by its icon. ↔ means rolling either way scrolls or moves the cursor.

With Settings > Flip screen on, the controls end up top right of the picture,
and so does the strip. Rolling toward the picture's left still does what
"wheel left" does below.

The firmware calls them Up (wheel left), Down (wheel right), Select (wheel
click) and Alt (power button). The power button is Back everywhere; hold it
to return to the watch face from any screen.

| Screen | Wheel left | Wheel right | Wheel click | Power button |
|---|---|---|---|---|
| Watch face | Notifications | Weather | Menu | Screen off |
| Menu, settings, notification list | Previous | Next | Open | Back |
| Notification (and the popup for new ones) | Scroll | Scroll | Actions (e.g. Clear) | Close |
| Music | Previous track, hold: volume up | Next track, hold: volume down | Play / pause | Back |
| Incoming call | Silence | Silence | Accept | Decline |
| Pairing code | | | Confirm | Cancel |
| Stopwatch (Menu) | Reset (stopped) | | Start / stop | Back |

While the phone has a track loaded, the watch face shows it in place of the
weekday.

## Restore the stock firmware

```bash
source ~/esp/esp-idf-v5.5.6/export.sh
python -m esptool --chip esp32s3 -b 921600 write_flash 0 backup/stock-full-flash.bin
```

`backup/stock-full-flash.bin` is a full dump of this watch taken before the
first flash. It is git-ignored because its NVS holds pairing keys.

## Layout

```
firmware/main/
  board/      pin map, efuse revision check, power latch
  drivers/    display (ST7735S), backlight, buttons, buzzer, RTC, battery
  ble/        NimBLE link and bonding, GATT queue, ANCS/AMS/CTS clients
  services/   settings (NVS), clock (UTC + POSIX time zone), phone (UI-side model)
  i18n/       Ukrainian/English strings
  ui/         LVGL glue, screen stack, theme, dithering, fonts, emoji filter, screens
  ui/art/     Earth and Moon renderers, land mask, sprites
  debug/      USB console (screenshots, button injection, demo data)
ios/          companion app (SwiftUI, CoreBluetooth); project.yml for XcodeGen
tools/        build/flash/console scripts, font/art/icon generators, clangd wrapper
design/       concept art
```

## Licence

GPL-3.0, see [LICENSE](LICENSE). Third-party fonts, data and references are
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
