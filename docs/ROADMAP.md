# Roadmap

## Milestone 1: base firmware (done, v0.1.0)

- ESP-IDF 5.5.6 project, stock bootloader kept, OTA-ready partition table
- Drivers: ST7735S display, backlight, buttons, buzzer, BM8563 RTC, battery and charging
- LVGL 9.5 UI with PragmataPro fonts covering Ukrainian and English
- Screens: watch face, menu, settings (language, brightness, screen timeout,
  12/24 h, flip, sounds, power off), stopwatch, about, notifications placeholder
- Screen-off after timeout, wake on any button
- USB console: screenshots, button injection, time sync

## Milestone 2: iPhone without an app (done, v0.2.0)

Apple exposes these BLE services to any paired accessory. Nothing to install on the phone.

- NimBLE peripheral advertising with the ANCS solicitation, LE Secure Connections
  bonding (numeric comparison on the watch), bonds in NVS, automatic reconnect
- ANCS: notification list, detail and popup with the phone's app names,
  dismiss/actions, incoming call screen with accept/decline and ring, chime
  on new notifications (respects the sounds setting)
- AMS: music screen (title, artist, progress, play/pause, next/prev, volume),
  now playing on the watch face
- CTS: time and UTC offset from the phone on every connect and change; a
  differing offset switches the watch to the phone's zone
- Emoji: about 250 mapped to icons, the rest dropped; bidi marks, skin tones
  and ZWJ sequences cleaned up; Latin Extended-A letters lose their accents
- Not done: LED alert per notification (no LED driver yet)

## Navigation hints

- Every screen shows what the wheel and the power button do, in a strip along
  the bottom edge next to them (`ui/hints.h`)
- Hold the power button to return to the watch face from anywhere; on the
  watch face a press turns the screen off
- With Flip screen on, the strip moves to the top right, where the controls
  then are, and the wheel's roll directions swap so left stays left

## Dithered UI (done, v0.3.0)

- Concept art from Codex image generation in design/concepts
- Ordered-dither engine (`ui/dither.h`): palette ramps, pattern tiles, canvases
- Orbit watch face: T+ mission days, Earth with the live day/night line, Moon
  in its real phase, craft position = time of day
- 1-bit PragmataPro, four-colour palette, checker-framed list selection,
  banner headers for notifications and calls, concept-style music screen

## Milestone 3: companion iOS app (SwiftUI + CoreBluetooth) (done, v0.4.1)

Checked on the iPhone 15 Pro Max: the app finds the bonded watch and
reconnects after relaunch; settings sync both ways; weather (Open-Meteo,
place name in Ukrainian) arrives on its own when the watch checks in;
find-my-watch rings and flashes; the 0.4.0 → 0.4.1 update went over BLE.
Not checked yet: weather refreshes while the app is in the background.

- Custom GATT service for settings, watch face choice and data push
- Weather for the current location
- Firmware updates over BLE into the spare OTA slot
- Background reconnects (`bluetooth-central` background mode)

## Milestone 4: health and motion

- Step counter: the LSM6DS3TR-C has a hardware pedometer, so counting costs no CPU
- Raise-to-wake via the IMU tilt interrupt
- Daily history in flash, synced to the app (optionally into Apple Health)

## Milestone 5: tools

- Alarms (BM8563 alarm interrupt), countdown timer, world clocks, flashlight

## Milestone 6: Ukraine-specific

- Air-raid alerts: the official "Повітряна тривога" iPhone app already pushes
  alerts, and ANCS forwards them to the watch. The watch recognises that app's
  notifications and shows a full-screen alert with siren and red LED. No server
  needed. The companion app can add an oblast filter and the all-clear state.
- Power-outage schedules (Yasno/DTEK queue) fetched by the companion app

## Milestone 7: battery life

- Light sleep between UI frames, lower CPU clock, display sleep, tuned BLE
  connection interval. Reviews report 2 to 3 hours from 600 mAh on stock firmware; the goal
  is a full day.
