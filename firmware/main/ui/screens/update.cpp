// Firmware update in progress (the companion app sends the image over BLE).
// The watch restarts into the new firmware by itself when it is done.
#include "screens.h"
#include "ui/dither.h"
#include "ui/theme.h"

#include "drivers/buzzer.h"
#include "i18n/i18n.h"
#include "services/ota.h"

#include <cstdio>

namespace {

constexpr int BarW = 116, BarH = 9;

class Update : public Screen {
public:
	Update() { ui::keepAwake(true); }
	~Update() override { ui::keepAwake(false); }

	void build(lv_obj_t* root) override {
		lv_obj_t* content = theme::titled(root, tr(Str::OtaTitle), LV_SYMBOL_DOWNLOAD);
		status = theme::label(content, &font_body, theme::text());
		lv_obj_set_width(status, 120);
		lv_label_set_long_mode(status, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 14);

		lv_obj_t* track = lv_obj_create(content);
		lv_obj_remove_style_all(track);
		lv_obj_set_size(track, BarW, BarH);
		lv_obj_align(track, LV_ALIGN_TOP_MID, 0, 46);
		lv_obj_set_style_border_width(track, 1, 0);
		lv_obj_set_style_border_color(track, theme::text(), 0);
		fill = lv_obj_create(track);
		lv_obj_remove_style_all(fill);
		lv_obj_set_pos(fill, 1, 1);
		lv_obj_set_size(fill, 0, BarH - 2);
		dither::fill(fill, 32, theme::accent(), theme::bg());

		percent = theme::label(content, &font_title, theme::text());
		lv_obj_align(percent, LV_ALIGN_TOP_MID, 0, 60);
		tick();
	}

	uint32_t tickMs() const override { return 200; }

	void tick() override {
		const float p = ota::progress();
		char buf[16];
		snprintf(buf, sizeof(buf), "%d%%", int(p * 100));
		lv_label_set_text(percent, buf);
		lv_obj_set_width(fill, int((BarW - 2) * (p > 1 ? 1 : p)));
		if (ota::failed()) {
			lv_label_set_text(status, tr(Str::OtaFailed));
			lv_obj_set_style_text_color(status, theme::accent(), 0);
			if (++failedTicks > 20) ui::pop();  // 4 s
			return;
		}
		lv_label_set_text(status, tr(p >= 0.999f ? (ota::active() ? Str::OtaVerifying : Str::OtaRestarting) : Str::OtaReceiving));
	}

	void onButton(const ButtonEvent& e) override {
		// The update goes on in the background; Back only hides this screen.
		if (e.button == Button::Alt && e.action == ButtonAction::Click) ui::pop();
	}

	hints::Set controls() const override { return { .back = LV_SYMBOL_LEFT }; }

private:
	lv_obj_t* status = nullptr;
	lv_obj_t* fill = nullptr;
	lv_obj_t* percent = nullptr;
	int failedTicks = 0;
};

// "Find my watch": flashes red and rings for 10 s, even with sounds off.
class Find : public Screen {
public:
	Find() { ui::keepAwake(true); }
	~Find() override { ui::keepAwake(false); }

	void build(lv_obj_t* root) override {
		this->root = root;
		lv_obj_t* text = theme::label(root, &font_large, theme::text(), tr(Str::FindHere));
		lv_obj_align(text, LV_ALIGN_CENTER, 0, -6);
		ticks = 0;
	}

	uint32_t tickMs() const override { return 250; }

	void tick() override {
		if (++ticks > 40) {
			ui::pop();
			return;
		}
		const bool on = ticks % 2;
		if (on) dither::fill(root, 40, theme::accent(), theme::bg());
		else {
			lv_obj_set_style_bg_image_src(root, nullptr, 0);
			lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
		}
		if (ticks % 4 == 1) ui::devices().buzzer.play({ { 2093, 120 }, { 0, 60 }, { 2637, 160 } }, true);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action == ButtonAction::Click) ui::pop();
	}

	hints::Set controls() const override { return { .back = LV_SYMBOL_CLOSE }; }

private:
	lv_obj_t* root = nullptr;
	int ticks = 0;
};

}  // namespace

std::unique_ptr<Screen> makeUpdate() {
	return std::make_unique<Update>();
}

std::unique_ptr<Screen> makeFind() {
	return std::make_unique<Find>();
}
