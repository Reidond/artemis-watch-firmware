# Third-party notices

This project is GPL-3.0 (see LICENSE). It includes or uses the following.

## Included in this repository

- **Iosevka** by Renzhi Li (Belleve Invis), SIL Open Font License 1.1.
  Glyph bitmaps generated from it are in `firmware/main/ui/fonts/font_*.c`
  (except `font_hint.c`). Source files come from Fontsource
  (`@fontsource/iosevka` 5.3.0) when `tools/gen_fonts.sh` runs.
- **Font Awesome 5 Free** by Fonticons, Inc.: icons CC BY 4.0, font SIL OFL
  1.1, as bundled with LVGL. Icon bitmaps are in the generated font files.
- **Natural Earth** 1:110m land polygons, public domain
  (https://www.naturalearthdata.com). Rasterized into
  `firmware/main/ui/art/earth_land.cpp` by `tools/gen_art.py`.
- **ST7735S initialisation values** follow LovyanGFX's `Panel_ST7735S`
  (https://github.com/lovyan03/LovyanGFX, FreeBSD licence).

## Used at build or run time, not included

- **LVGL** (MIT), fetched by the ESP-IDF component manager.
- **ESP-IDF** and **NimBLE** (Apache-2.0), the toolchain and BLE stack.
- **lv_font_conv** (MIT), run through npx to generate fonts.
- **Open-Meteo** weather API (https://open-meteo.com), data under CC BY 4.0;
  the companion app credits it.

## References

- CircuitMess GC_Artemis-Firmware (https://github.com/CircuitMess/GC_Artemis-Firmware):
  pin map, display configuration, battery calibration and bootloader
  behaviour of the watch were learned from it. This firmware is a separate
  implementation.

"NASA", "Artemis" and "CircuitMess" are names of their respective owners;
this project is not affiliated with or endorsed by them.
