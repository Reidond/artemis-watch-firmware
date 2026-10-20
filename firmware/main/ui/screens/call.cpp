// Incoming call (ANCS IncomingCall): Select accepts, Back declines, Up/Down
// silence the watch. Closes
// itself when the phone drops the call notification (answered, declined,
// hung up).
#include "screens.h"
#include "ui/dither.h"
#include "ui/theme.h"

#include "drivers/buzzer.h"
#include "i18n/i18n.h"

#include <algorithm>
#include <cmath>
#include <esp_timer.h>

namespace {

class Call;
Call* openCall = nullptr;

constexpr uint32_t RingEveryMs = 2000;

uint32_t nowMs() {
	return uint32_t(esp_timer_get_time() / 1000);
}

class Call : public Screen {
public:
	explicit Call(uint32_t uid) : uid(uid) {
		openCall = this;
		ui::keepAwake(true);
	}
	~Call() override {
		if (openCall == this) openCall = nullptr;
		ui::keepAwake(false);
	}

	void build(lv_obj_t* root) override {
		shownName.clear();
		lv_obj_t* content = theme::banner(root, tr(Str::IncomingCall), LV_SYMBOL_CALL);

		// Dithered rings pulsing out from the handset while it rings.
		lv_obj_t* waves = rings.create(content, 128, RingsH);
		lv_obj_set_pos(waves, 0, 0);
		lv_obj_t* icon = theme::label(content, &font_large, theme::text(), LV_SYMBOL_CALL);
		lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, RingsH / 2 - 11);
		pulse = 0;
		drawRings();

		name = theme::label(content, &font_large, theme::text());
		lv_obj_set_size(name, 124, 2 * lv_font_get_line_height(&font_large));
		lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_align(name, LV_ALIGN_TOP_MID, 0, RingsH + 2);
		refresh();
	}

	uint32_t tickMs() const override { return 250; }

	void tick() override {
		if (!phone::find(uid)) {
			ui::pop();
			return;
		}
		refresh();
		if (!answered) {
			pulse = (pulse + 1) % 8;
			drawRings();
		}
		const uint32_t now = nowMs();
		if (!quiet && now - lastRing >= RingEveryMs) {
			lastRing = now;
			ui::devices().buzzer.ring();
		}
	}

	void drawRings() {
		rings.clear(theme::bg());
		const float cx = 63.5f, cy = RingsH / 2.0f;
		for (int y = 0; y < RingsH; y++) {
			for (int x = 0; x < 128; x++) {
				const float d = std::hypot(x - cx, y - cy);
				// Bands every 8 px moving outward, fading with distance.
				const float band = std::fmod(d - pulse, 8.0f);
				if (d < 14 || band > 2.0f) continue;
				const float v = std::max(0.0f, 0.7f - (d - 14) / 70.0f);
				rings.shade(x, y, v, dither::red());
			}
		}
		rings.commit();
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action != ButtonAction::Click) return;
		const phone::Notification* n = phone::find(uid);
		switch (e.button) {
			case Button::Select:
				if (n && (n->flags & phone::flag::PositiveAction) && !answered) phone::perform(uid, true);
				answered = quiet = true;
				break;
			case Button::Alt:
				// A second Back closes the screen even if the phone has not reacted.
				if (answered) {
					ui::pop();
					return;
				}
				if (n && (n->flags & phone::flag::NegativeAction)) phone::perform(uid, false);
				answered = quiet = true;
				break;
			default:
				quiet = true;  // Up / Down: silence the watch, keep the call
				break;
		}
		refresh();
	}

	hints::Set controls() const override {
		const phone::Notification* n = phone::find(uid);
		hints::Set s;
		if (!quiet) s.up = s.down = LV_SYMBOL_MUTE;
		if (n && (n->flags & phone::flag::PositiveAction) && !answered) s.press = { LV_SYMBOL_CALL, theme::good() };
		s.back = answered ? hints::Hint(LV_SYMBOL_LEFT) : hints::Hint(LV_SYMBOL_CLOSE, theme::accent());
		return s;
	}

private:
	void refresh() {
		const phone::Notification* n = phone::find(uid);
		const std::string caller = n && !n->title.empty() ? n->title : tr(Str::UnknownCaller);
		if (caller != shownName) {
			shownName = caller;
			lv_label_set_text(name, caller.c_str());
		}
	}

	uint32_t uid;
	bool answered = false;
	bool quiet = false;
	uint32_t lastRing = 0;
	std::string shownName;
	lv_obj_t* name = nullptr;
	static constexpr int RingsH = 50;
	dither::Canvas rings;
	int pulse = 0;
};

}  // namespace

std::unique_ptr<Screen> makeCall(uint32_t uid) {
	return std::make_unique<Call>(uid);
}

bool callScreenOpen() {
	return openCall != nullptr;
}
