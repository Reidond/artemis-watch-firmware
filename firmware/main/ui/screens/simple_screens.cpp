// Small screens: about and the power-off / forget-phone confirmations.
#include "screens.h"
#include "ui/theme.h"

#include "board/board.h"
#include "drivers/battery.h"
#include "i18n/i18n.h"
#include "services/phone.h"

#include <cstdio>
#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

namespace {

class About : public Screen {
public:
	void build(lv_obj_t* root) override {
		content = theme::titled(root, tr(Str::MenuAbout));
		lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
		lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_style_pad_left(content, 4, 0);
		lv_obj_set_style_pad_top(content, 2, 0);
		const Str labels[Rows] = { Str::AboutFirmware, Str::AboutHardware, Str::AboutBattery, Str::AboutUptime, Str::AboutMemory };
		for (int i = 0; i < Rows; i++) {
			theme::label(content, &font_small, theme::muted(), tr(labels[i]));
			values[i] = theme::label(content, &font_body, theme::text());
			lv_obj_set_style_margin_bottom(values[i], 2, 0);
		}
	}

	void tick() override {
		auto& dev = ui::devices();
		char buf[48];
		lv_label_set_text(values[0], esp_app_get_description()->version);

		snprintf(buf, sizeof(buf), "rev %u", board::identity().revision);
		lv_label_set_text(values[1], buf);

		const Charge charge = dev.battery.charge();
		if (charge == Charge::Unplugged && dev.battery.percent() >= 0) {
			snprintf(buf, sizeof(buf), "%d%%  %d mV", dev.battery.percent(), dev.battery.millivolts());
		} else {
			snprintf(buf, sizeof(buf), "%s", tr(charge == Charge::Charging ? Str::AboutCharging : Str::AboutCharged));
		}
		lv_label_set_text(values[2], buf);

		const int64_t up = esp_timer_get_time() / 1000000;
		snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", up / 3600, (up / 60) % 60, up % 60);
		lv_label_set_text(values[3], buf);

		snprintf(buf, sizeof(buf), "%u + %u KB", unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
			unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
		lv_label_set_text(values[4], buf);
	}

	void onButton(const ButtonEvent& e) override {
		const bool step = e.action == ButtonAction::Press || e.action == ButtonAction::Repeat;
		if (e.button == Button::Up && step) lv_obj_scroll_by_bounded(content, 0, 30, LV_ANIM_ON);
		if (e.button == Button::Down && step) lv_obj_scroll_by_bounded(content, 0, -30, LV_ANIM_ON);
		if (e.button == Button::Alt && e.action == ButtonAction::Click) ui::pop();
	}

	hints::Set controls() const override { return { hints::Move, hints::Move, nullptr, LV_SYMBOL_LEFT }; }

private:
	static constexpr int Rows = 5;
	lv_obj_t* content = nullptr;
	lv_obj_t* values[Rows] = {};
};

// "Do it?" with Select = yes, Back = no.
class Confirm : public Screen {
public:
	Confirm(const char* icon, Str question, void (*yes)()) : icon(icon), question(question), yes(yes) {}

	void build(lv_obj_t* root) override {
		lv_obj_t* i = theme::label(root, &font_large, theme::accent(), icon);
		lv_obj_align(i, LV_ALIGN_CENTER, 0, -18);
		lv_obj_t* q = theme::label(root, &font_large, theme::text(), tr(question));
		lv_obj_align(q, LV_ALIGN_CENTER, 0, 10);
	}

	hints::Set controls() const override { return { .press = LV_SYMBOL_OK, .back = LV_SYMBOL_CLOSE }; }

	void onButton(const ButtonEvent& e) override {
		if (e.action != ButtonAction::Click) return;
		if (e.button == Button::Select) {
			yes();
			ui::pop();
		}
		if (e.button == Button::Alt) ui::pop();
	}

private:
	const char* icon;
	Str question;
	void (*yes)();
};

}  // namespace

std::unique_ptr<Screen> makeAbout() {
	return std::make_unique<About>();
}

std::unique_ptr<Screen> makePowerOff() {
	return std::make_unique<Confirm>(LV_SYMBOL_POWER, Str::PowerOffConfirm, [] { board::powerOff(); });
}

std::unique_ptr<Screen> makeForgetPhone() {
	return std::make_unique<Confirm>(LV_SYMBOL_BLUETOOTH, Str::ForgetPhoneConfirm, [] { phone::forget(); });
}
