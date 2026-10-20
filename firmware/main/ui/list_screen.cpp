#include "list_screen.h"
#include "dither.h"
#include "theme.h"

namespace {
constexpr int RowHeight = 20;
constexpr int RowHeightWithValue = 38;
constexpr int Frame = 2;     // checker border around the selected row
constexpr int MarkRoom = 7;  // the selected row's ▶
}  // namespace

void ListScreen::build(lv_obj_t* root) {
	this->root = root;
	list = items();
	rows.clear();
	values.clear();
	texts.clear();
	textWidths.clear();
	icons.clear();
	marks.clear();

	content = theme::titled(root, title().c_str());
	lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
	lv_obj_set_scroll_dir(content, LV_DIR_VER);
	lv_obj_set_style_pad_top(content, 1, 0);
	lv_obj_set_style_pad_bottom(content, 1, 0);

	for (const auto& item : list) {
		const bool hasValue = bool(item.value);
		const int h = hasValue ? RowHeightWithValue : RowHeight;

		// Row: carries the checker frame when selected. Plate: black inside it.
		lv_obj_t* row = lv_obj_create(content);
		lv_obj_remove_style_all(row);
		lv_obj_set_size(row, 124, h);
		lv_obj_set_style_margin_left(row, 2, 0);
		lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

		lv_obj_t* plate = lv_obj_create(row);
		lv_obj_remove_style_all(plate);
		lv_obj_set_pos(plate, Frame, Frame);
		lv_obj_set_size(plate, 124 - 2 * Frame, h - 2 * Frame);
		lv_obj_set_style_bg_color(plate, theme::bg(), 0);
		lv_obj_set_style_bg_opa(plate, LV_OPA_COVER, 0);
		lv_obj_remove_flag(plate, LV_OBJ_FLAG_SCROLLABLE);

		int textX = 3;
		lv_obj_t* icon = nullptr;
		if (item.icon) {
			icon = theme::label(plate, &font_body, theme::text(), item.icon);
			lv_obj_set_width(icon, 16);
			lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
			lv_obj_align(icon, LV_ALIGN_LEFT_MID, 2, 0);
			textX = 20;
		}
		const int textW = 124 - 2 * Frame - textX - 2;
		lv_obj_t* text = theme::label(plate, &font_body, theme::text(), item.label().c_str());
		// DOTS/scroll modes need a fixed height, otherwise the label wraps.
		lv_obj_set_size(text, textW, lv_font_get_line_height(&font_body));
		lv_label_set_long_mode(text, LV_LABEL_LONG_MODE_DOTS);
		if (hasValue) {
			lv_obj_set_pos(text, textX, 0);
			lv_obj_t* value = theme::label(plate, &font_small, theme::muted(), "");
			lv_obj_set_size(value, textW, lv_font_get_line_height(&font_small));
			lv_label_set_long_mode(value, LV_LABEL_LONG_MODE_DOTS);
			lv_obj_set_pos(value, textX, lv_font_get_line_height(&font_body));
			values.push_back(value);
		} else {
			lv_obj_align(text, LV_ALIGN_LEFT_MID, textX, 0);
			values.push_back(nullptr);
		}

		lv_obj_t* mark = theme::label(plate, &font_hint, theme::accent(), LV_SYMBOL_PLAY);
		lv_obj_align(mark, LV_ALIGN_RIGHT_MID, -1, 0);

		// Red bar over the frame's left edge.
		lv_obj_t* bar = lv_obj_create(row);
		lv_obj_remove_style_all(bar);
		lv_obj_set_size(bar, 2, h);
		lv_obj_set_style_bg_color(bar, theme::accent(), 0);
		lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
		lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);

		rows.push_back(row);
		texts.push_back(text);
		textWidths.push_back(textW);
		icons.push_back(icon);
		marks.push_back({ mark, bar });
	}
	refreshValues();
	if (selected >= int(rows.size())) selected = 0;
	select(selected, false);
	decorate(root);
}

void ListScreen::reload() {
	if (!root) return;
	lv_obj_clean(root);
	build(root);
}

void ListScreen::refreshValues() {
	for (size_t i = 0; i < list.size(); i++) {
		if (values[i]) lv_label_set_text(values[i], list[i].value().c_str());
	}
}

void ListScreen::select(int index, bool animate) {
	if (rows.empty()) return;
	const int n = int(rows.size());
	selected = (index % n + n) % n;
	for (int i = 0; i < n; i++) {
		const bool on = i == selected;
		if (on) dither::fill(rows[i], 32, theme::text(), theme::bg());
		else lv_obj_set_style_bg_image_src(rows[i], nullptr, 0);
		lv_obj_set_flag(marks[i].first, LV_OBJ_FLAG_HIDDEN, !on);
		lv_obj_set_flag(marks[i].second, LV_OBJ_FLAG_HIDDEN, !on);
		if (icons[i]) lv_obj_set_style_text_color(icons[i], on ? theme::accent() : theme::text(), 0);
		// Selected row scrolls its label so long names are readable, and
		// leaves room for the ▶ mark.
		lv_label_set_long_mode(texts[i], on ? LV_LABEL_LONG_MODE_SCROLL : LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_width(texts[i], textWidths[i] - (on ? MarkRoom : 0));
		if (values[i]) lv_obj_set_style_text_color(values[i], on ? theme::text() : theme::muted(), 0);
	}
	lv_obj_scroll_to_view(rows[selected], animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

hints::Set ListScreen::controls() const {
	if (list.empty()) return { .back = LV_SYMBOL_LEFT };
	return { hints::Move, hints::Move, LV_SYMBOL_OK, LV_SYMBOL_LEFT };
}

void ListScreen::onButton(const ButtonEvent& e) {
	const bool step = e.action == ButtonAction::Press || e.action == ButtonAction::Repeat;
	switch (e.button) {
		case Button::Up:
			if (step) select(selected - 1, true);
			break;
		case Button::Down:
			if (step) select(selected + 1, true);
			break;
		case Button::Select:
			if (e.action == ButtonAction::Click && selected < int(list.size()) && list[selected].action) {
				list[selected].action();
				refreshValues();
				select(selected, false);
			}
			break;
		case Button::Alt:
			if (e.action == ButtonAction::Click) ui::pop();
			break;
	}
}
