#include "hints.h"
#include "theme.h"

#define ARROW_LEFT  "\xEF\x81\xA0"  // U+F060
#define ARROW_RIGHT "\xEF\x81\xA1"  // U+F061
#define ARROW_BOTH  "\xEF\x8C\xB7"  // U+F337 arrows-alt-h

namespace hints {

namespace {

constexpr int Gap = 6;        // between the wheel's actions
constexpr int PowerGap = 10;  // before the power button's
constexpr lv_obj_flag_t Tag = LV_OBJ_FLAG_USER_1;  // marks the strip among the root's children

bool flipped = false;

// Which control (arrow, dot, power sign) is dimmer than what it does.
lv_color_t dim() {
	return lv_color_hex(0x6A707A);
}

lv_obj_t* find(lv_obj_t* root) {
	const uint32_t n = lv_obj_get_child_count(root);
	for (uint32_t i = 0; i < n; i++) {
		lv_obj_t* c = lv_obj_get_child(root, int32_t(i));
		if (lv_obj_has_flag(c, Tag)) return c;
	}
	return nullptr;
}

lv_obj_t* create(lv_obj_t* root) {
	lv_obj_t* strip = lv_obj_create(root);
	lv_obj_remove_style_all(strip);
	lv_obj_add_flag(strip, Tag);
	lv_obj_add_flag(strip, LV_OBJ_FLAG_FLOATING);
	lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_remove_flag(strip, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_size(strip, 128, Height);
	// In the padding reserve() left, outside the root's content area.
	if (flipped) lv_obj_align(strip, LV_ALIGN_TOP_RIGHT, 0, -Height);
	else lv_obj_align(strip, LV_ALIGN_BOTTOM_LEFT, 0, Height);
	// Opaque, so content that runs into it never shows through.
	lv_obj_set_style_bg_color(strip, theme::bg(), 0);
	lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
	if (flipped) lv_obj_set_style_pad_right(strip, 3, 0);
	else lv_obj_set_style_pad_left(strip, 3, 0);
	lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(strip, flipped ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
		LV_FLEX_ALIGN_CENTER);
	return strip;
}

lv_obj_t* glyph(lv_obj_t* strip, const char* text, lv_color_t color) {
	lv_obj_t* l = lv_label_create(strip);
	lv_obj_set_style_text_font(l, &font_hint, 0);
	lv_obj_set_style_text_color(l, color, 0);
	lv_label_set_text(l, text);
	return l;
}

lv_obj_t* dot(lv_obj_t* strip) {
	lv_obj_t* d = lv_obj_create(strip);
	lv_obj_remove_style_all(d);
	lv_obj_set_size(d, 5, 5);
	lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
	lv_obj_set_style_bg_color(d, dim(), 0);
	lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
	return d;
}

// `control` (already added) followed by what it does.
void action(lv_obj_t* strip, lv_obj_t* control, const Hint& h, int gap) {
	if (lv_obj_get_index(control) > 0) lv_obj_set_style_margin_left(control, gap, 0);
	if (!*h.icon) return;
	lv_obj_t* l = glyph(strip, h.icon, h.colored ? h.color : theme::muted());
	lv_obj_set_style_margin_left(l, 2, 0);
}

// In the order the controls sit: the wheel, then the power button, or the
// other way round when flipped.
void draw(lv_obj_t* strip, const Set& s) {
	lv_obj_clean(strip);
	const auto power = [&] {
		if (s.back.icon) action(strip, glyph(strip, LV_SYMBOL_POWER, dim()), s.back, PowerGap);
	};
	if (flipped) power();
	int gap = PowerGap;  // before the wheel's first action, if it follows the power button's
	const auto wheel = [&](lv_obj_t* control, const Hint& h) {
		action(strip, control, h, gap);
		gap = Gap;
	};
	if (s.up.icon && s.up == s.down) {
		wheel(glyph(strip, ARROW_BOTH, dim()), s.up);
	} else {
		if (s.up.icon) wheel(glyph(strip, ARROW_LEFT, dim()), s.up);
		if (s.down.icon) wheel(glyph(strip, ARROW_RIGHT, dim()), s.down);
	}
	if (s.press.icon) wheel(dot(strip), s.press);
	if (!flipped) power();
	if (lv_obj_get_child_count(strip)) lv_obj_remove_flag(strip, LV_OBJ_FLAG_HIDDEN);
	else lv_obj_add_flag(strip, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

void setFlipped(bool f) {
	flipped = f;
}

void reserve(lv_obj_t* root) {
	lv_obj_set_style_pad_top(root, flipped ? Height : 0, 0);
	lv_obj_set_style_pad_bottom(root, flipped ? 0 : Height, 0);
}

void apply(lv_obj_t* root, const Set& set, Set& shown) {
	lv_obj_t* strip = find(root);
	if (!strip) {
		strip = create(root);
		draw(strip, set);
		shown = set;
	} else if (!(set == shown)) {
		draw(strip, set);
		shown = set;
	}
	if (lv_obj_get_index(strip) != int32_t(lv_obj_get_child_count(root)) - 1) lv_obj_move_foreground(strip);
}

}  // namespace hints
