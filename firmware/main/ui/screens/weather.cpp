// Weather pushed by the companion app: now, today's range and the next hours.
#include "screens.h"
#include "ui/fonts/icons.h"
#include "ui/theme.h"

#include "i18n/i18n.h"
#include "services/clock.h"
#include "services/weather.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr int Columns = 5;  // "18°" is 3 glyphs (24 px) wide

bool isNight(int hour) {
	return hour >= 20 || hour < 6;
}

lv_obj_t* pixelArt(lv_obj_t* parent, const lv_image_dsc_t* src, int scale) {
	lv_obj_t* img = lv_image_create(parent);
	lv_image_set_src(img, src);
	lv_image_set_antialias(img, false);
	lv_image_set_pivot(img, 0, 0);
	lv_image_set_scale(img, 256 * scale);
	lv_obj_set_size(img, src->header.w * scale, src->header.h * scale);
	return img;
}

class Weather : public Screen {
public:
	void build(lv_obj_t* root) override {
		content = theme::titled(root, tr(Str::MenuWeather), ICON_CLOUD);
		shown = -1;
	}

	uint32_t tickMs() const override { return 5000; }

	void tick() override {
		const auto* w = weather::current();
		const int64_t stamp = w ? w->ts : 0;
		if (stamp == shown) return;
		shown = stamp;
		lv_obj_clean(content);
		if (!w) {
			showEmpty();
			return;
		}
		const int hour = clock_svc::local().tm_hour;
		char buf[64];

		pixelArt(content, weather::icon(w->code, isNight(hour)), 2);
		lv_obj_set_pos(lv_obj_get_child(content, -1), 4, 2);

		snprintf(buf, sizeof(buf), "%ld°", std::lround(w->temp));
		lv_obj_t* temp = theme::label(content, &font_large, theme::text(), buf);
		lv_obj_set_pos(temp, 42, 1);
		lv_obj_t* cond = theme::label(content, &font_small, theme::muted(), tr(weather::condition(w->code)));
		lv_obj_set_size(cond, 84, lv_font_get_line_height(&font_small));
		lv_label_set_long_mode(cond, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
		lv_obj_set_pos(cond, 42, 21);

		snprintf(buf, sizeof(buf), LV_SYMBOL_UP "%ld° " LV_SYMBOL_DOWN "%ld°", std::lround(w->hi), std::lround(w->lo));
		lv_obj_t* range = theme::label(content, &font_small, theme::text(), buf);
		lv_obj_set_pos(range, 4, 37);
		lv_obj_t* place = theme::label(content, &font_small, theme::accent(), w->place);
		lv_obj_set_size(place, 120, lv_font_get_line_height(&font_small));
		lv_label_set_long_mode(place, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_pos(place, 4, 52);

		// Next hours: hour, icon, temperature.
		const int n = w->hourCount < Columns ? w->hourCount : Columns;
		for (int i = 0; i < n; i++) {
			const auto& h = w->hours[i];
			const int x = 1 + i * 25;
			snprintf(buf, sizeof(buf), "%02d", h.hour);
			lv_obj_t* hl = theme::label(content, &font_small, theme::muted(), buf);
			lv_obj_set_width(hl, 25);
			lv_obj_set_style_text_align(hl, LV_TEXT_ALIGN_CENTER, 0);
			lv_obj_set_pos(hl, x, 68);
			snprintf(buf, sizeof(buf), "%d°", h.temp);
			lv_obj_t* tl = theme::label(content, &font_small, theme::text(), buf);
			lv_obj_set_width(tl, 25);
			lv_obj_set_style_text_align(tl, LV_TEXT_ALIGN_CENTER, 0);
			lv_obj_set_pos(tl, x, 82);
		}
	}

	void onButton(const ButtonEvent& e) override {
		if (e.button == Button::Alt && e.action == ButtonAction::Click) ui::pop();
	}

	hints::Set controls() const override { return { .back = LV_SYMBOL_LEFT }; }

private:
	void showEmpty() {
		lv_obj_t* text = theme::label(content, &font_body, theme::text(), tr(Str::WeatherNone));
		lv_obj_set_width(text, 124);
		lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_align(text, LV_ALIGN_TOP_MID, 0, 30);
		lv_obj_t* hint = theme::label(content, &font_small, theme::muted(), tr(Str::WeatherHint));
		lv_obj_set_width(hint, 120);
		lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 52);
	}

	lv_obj_t* content = nullptr;
	int64_t shown = -1;
};

}  // namespace

std::unique_ptr<Screen> makeWeather() {
	return std::make_unique<Weather>();
}
