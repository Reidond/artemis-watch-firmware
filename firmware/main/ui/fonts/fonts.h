#pragma once

#include <lvgl.h>

// Generated from PragmataPro by tools/gen_fonts.sh, 1 bit per pixel (no
// antialiasing) for the dithered look. Text fonts include Cyrillic and the
// LV_SYMBOL_* icons.
LV_FONT_DECLARE(font_small)  // 14 px regular: secondary text, values
LV_FONT_DECLARE(font_label)  // 14 px bold: counters, short emphasis
LV_FONT_DECLARE(font_body)   // 15 px regular: list items, body text (12 Cyrillic letters in 90 px)
LV_FONT_DECLARE(font_title)  // 16 px bold: screen titles
LV_FONT_DECLARE(font_large)  // 20 px bold: headings, emphasis
LV_FONT_DECLARE(font_clock)  // 40 px bold digits, ':' '.' '-' '/' and space only
LV_FONT_DECLARE(font_hint)   // 10 px FontAwesome icons for the control hints (ui/hints.h)
