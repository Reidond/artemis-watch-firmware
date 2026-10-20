#pragma once

#include <lvgl.h>

// On-screen hints for the watch's two controls, both on the bottom edge left
// of the display: the wheel (roll left = Button::Up, roll right =
// Button::Down, click = Button::Select) and, to its right, the power button
// (Button::Alt). Every screen shows what they do in a strip along the bottom,
// left-aligned so the icons sit next to the controls.
//
// Settings > Flip screen turns the picture 180°, which puts the controls
// top-right of it, power button first. The strip follows them there, and
// ui.cpp swaps Up and Down so rolling toward the picture's left is still Up.

#define HINT_MORE "\xEF\x85\x81"  // U+F141 ellipsis: actions menu

namespace hints {

constexpr int Height = 12;

// For up/down: they just move a cursor or scroll. Drawn as a bare arrow, and
// as one "↔" when both do.
inline constexpr const char* Move = "";

struct Hint {
	Hint(const char* icon = nullptr) : icon(icon) {}
	Hint(const char* icon, lv_color_t color) : icon(icon), color(color), colored(true) {}
	bool operator==(const Hint& o) const {
		return icon == o.icon && colored == o.colored && (!colored || lv_color_eq(color, o.color));
	}

	const char* icon;  // LV_SYMBOL_* in font_hint, Move, or nullptr: does nothing here
	lv_color_t color{};
	bool colored = false;
};

struct Set {
	Hint up;     // wheel left
	Hint down;   // wheel right
	Hint press;  // wheel click
	Hint back;   // power button
	bool operator==(const Set& o) const = default;
};

// Which edge the strip goes on. Takes effect for roots created afterwards.
void setFlipped(bool flipped);

// Pads a new screen root so its content area is the part the strip leaves
// free: y 0-116 normally, 12-128 flipped.
void reserve(lv_obj_t* root);

// Strip shown by the screen stack (ui.cpp): finds the one on `root` or creates
// it, redraws it if `set` differs from `shown`, and keeps it on top.
void apply(lv_obj_t* root, const Set& set, Set& shown);

}  // namespace hints
