#pragma once

#include "ui/fonts/fonts.h"
#include <lvgl.h>

namespace theme {

// Palette of design/concepts: black, off-white, NASA red, grey. Every other
// shade is a dither pattern of these (ui/dither.h).
inline lv_color_t bg() { return lv_color_hex(0x000000); }
inline lv_color_t text() { return lv_color_hex(0xF2F2F2); }
inline lv_color_t muted() { return lv_color_hex(0x808080); }
inline lv_color_t accent() { return lv_color_hex(0xFC3D21); }     // NASA red
inline lv_color_t good() { return lv_color_hex(0xF2F2F2); }     // positive actions (answer, confirm)
inline lv_color_t link() { return lv_color_hex(0xF2F2F2); }       // Bluetooth connected

// A full-screen black container with no scrolling, used as screen root. Its
// content area (128 x 116) leaves out the control hints (ui/hints.h), which
// are at the bottom or, with the screen flipped, at the top.
lv_obj_t* screen();
constexpr int TitleHeight = 22;
// Bold left-aligned title (optional red icon before it) over a dotted rule.
// Returns the content area below it, which ends at the control hints or the
// screen's bottom edge.
lv_obj_t* titled(lv_obj_t* screen, const char* title, const char* icon = nullptr);
// Header band for things that arrive (notifications, calls): title in a black
// plate inside a checker frame (design/concepts/05-notification.png).
lv_obj_t* banner(lv_obj_t* screen, const char* title, const char* icon = nullptr);
lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text = "");

}  // namespace theme
