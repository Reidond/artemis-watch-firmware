#include "screens.h"
#include "ui/theme.h"

#include "drivers/buzzer.h"
#include "i18n/i18n.h"

#include <cstdio>
#include <esp_timer.h>

namespace {

// Keeps running in the background after leaving the screen.
struct StopwatchState {
	bool running = false;
	int64_t startedUs = 0;
	int64_t accumulatedUs = 0;

	int64_t elapsedUs() const {
		return accumulatedUs + (running ? esp_timer_get_time() - startedUs : 0);
	}
} state;

class Stopwatch : public Screen {
public:
	void build(lv_obj_t* root) override {
		lv_obj_t* content = theme::titled(root, tr(Str::MenuStopwatch));
		main = theme::label(content, &font_clock, theme::text());
		lv_obj_set_width(main, 128);
		lv_obj_set_style_text_align(main, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_align(main, LV_ALIGN_TOP_MID, 0, 6);
		hundredths = theme::label(content, &font_large, theme::accent());
		lv_obj_set_width(hundredths, 128);
		lv_obj_set_style_text_align(hundredths, LV_TEXT_ALIGN_CENTER, 0);
		lv_obj_align(hundredths, LV_ALIGN_TOP_MID, 0, 50);
	}

	hints::Set controls() const override {
		const bool reset = !state.running && state.accumulatedUs;
		return { reset ? LV_SYMBOL_REFRESH : nullptr, nullptr, state.running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY,
			LV_SYMBOL_LEFT };
	}

	uint32_t tickMs() const override { return state.running ? 50 : 500; }

	void tick() override {
		const int64_t ms = state.elapsedUs() / 1000;
		const int64_t totalSec = ms / 1000;
		char buf[32];
		if (totalSec >= 3600) {
			snprintf(buf, sizeof(buf), "%lld:%02lld", totalSec / 3600, (totalSec / 60) % 60);
		} else {
			snprintf(buf, sizeof(buf), "%02lld:%02lld", totalSec / 60, totalSec % 60);
		}
		lv_label_set_text(main, buf);
		snprintf(buf, sizeof(buf), ".%02lld", (ms / 10) % 100);
		lv_label_set_text(hundredths, totalSec >= 3600 ? "" : buf);
		lv_obj_set_style_text_color(main, state.running ? theme::text() : theme::muted(), 0);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action != ButtonAction::Click) return;
		switch (e.button) {
			case Button::Select:
				if (state.running) {
					state.accumulatedUs += esp_timer_get_time() - state.startedUs;
					state.running = false;
				} else {
					state.startedUs = esp_timer_get_time();
					state.running = true;
				}
				ui::devices().buzzer.click();
				break;
			case Button::Up:
				if (!state.running) state.accumulatedUs = 0;
				break;
			case Button::Alt:
				ui::pop();
				return;
			default:
				break;
		}
		tick();
	}

private:
	lv_obj_t* main = nullptr;
	lv_obj_t* hundredths = nullptr;
};

}  // namespace

std::unique_ptr<Screen> makeStopwatch() {
	return std::make_unique<Stopwatch>();
}
