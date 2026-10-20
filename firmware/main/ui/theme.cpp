#include "theme.h"
#include "dither.h"
#include "hints.h"

namespace theme {

lv_obj_t* screen() {
	lv_obj_t* s = lv_obj_create(nullptr);
	lv_obj_remove_style_all(s);
	lv_obj_set_size(s, 128, 128);
	lv_obj_set_style_bg_color(s, bg(), 0);
	lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
	lv_obj_set_style_text_color(s, text(), 0);
	lv_obj_set_style_text_font(s, &font_body, 0);
	lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
	hints::reserve(s);
	return s;
}

lv_obj_t* titled(lv_obj_t* scr, const char* title, const char* icon) {
	int x = 5;
	if (icon) {
		lv_obj_t* i = label(scr, &font_title, accent(), icon);
		lv_obj_set_pos(i, x, 1);
		lv_obj_update_layout(i);
		x += lv_obj_get_width(i) + 4;
	}
	lv_obj_t* l = label(scr, &font_title, text(), title);
	lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(l, 124 - x, lv_font_get_line_height(&font_title));
	lv_obj_set_pos(l, x, 2);

	// Dotted rule under the title, the printout look of the concepts.
	lv_obj_t* rule = lv_obj_create(scr);
	lv_obj_remove_style_all(rule);
	lv_obj_set_pos(rule, 0, TitleHeight - 2);
	lv_obj_set_size(rule, 128, 1);
	dither::fill(rule, 24, muted(), bg());

	lv_obj_t* content = lv_obj_create(scr);
	lv_obj_remove_style_all(content);
	lv_obj_set_pos(content, 0, TitleHeight);
	lv_obj_set_size(content, 128, 128 - TitleHeight - hints::Height);
	return content;
}

lv_obj_t* banner(lv_obj_t* scr, const char* title, const char* icon) {
	lv_obj_t* band = lv_obj_create(scr);
	lv_obj_remove_style_all(band);
	lv_obj_set_size(band, 128, TitleHeight);
	dither::fill(band, 32, text(), bg());

	lv_obj_t* plate = lv_obj_create(band);
	lv_obj_remove_style_all(plate);
	lv_obj_set_pos(plate, 3, 2);
	lv_obj_set_size(plate, 122, TitleHeight - 4);
	lv_obj_set_style_bg_color(plate, bg(), 0);
	lv_obj_set_style_bg_opa(plate, LV_OPA_COVER, 0);

	int x = 3;
	if (icon) {
		lv_obj_t* i = label(plate, &font_body, text(), icon);
		lv_obj_set_pos(i, x, 0);
		lv_obj_update_layout(i);
		x += lv_obj_get_width(i) + 4;
	}
	lv_obj_t* l = label(plate, &font_title, text(), title);
	lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
	lv_obj_set_size(l, 120 - x, lv_font_get_line_height(&font_title));
	lv_obj_set_pos(l, x, 0);

	lv_obj_t* content = lv_obj_create(scr);
	lv_obj_remove_style_all(content);
	lv_obj_set_pos(content, 0, TitleHeight);
	lv_obj_set_size(content, 128, 128 - TitleHeight - hints::Height);
	return content;
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* txt) {
	lv_obj_t* l = lv_label_create(parent);
	lv_obj_set_style_text_font(l, font, 0);
	lv_obj_set_style_text_color(l, color, 0);
	lv_label_set_text(l, txt);
	return l;
}

}  // namespace theme
