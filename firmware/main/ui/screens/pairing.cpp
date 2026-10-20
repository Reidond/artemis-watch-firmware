// Pairing with the iPhone. Numeric comparison: both show the same six digits,
// Select on the watch (and Pair on the phone) confirms. Also shows the result.
#include "screens.h"
#include "ui/theme.h"

#include "i18n/i18n.h"

#include <cstdio>
#include <esp_timer.h>

namespace {

class Pairing;
Pairing* openPairing = nullptr;

uint32_t nowMs() {
	return uint32_t(esp_timer_get_time() / 1000);
}

class Pairing : public Screen {
public:
	enum class State : uint8_t { Asking, Waiting, Done };

	Pairing(uint32_t code, bool confirm) : code(code), confirm(confirm) {
		openPairing = this;
		ui::keepAwake(true);
	}
	explicit Pairing(phone::PairResult result) : state(State::Done), result(result), doneAt(nowMs()) {
		openPairing = this;
		ui::keepAwake(true);
	}
	~Pairing() override {
		if (openPairing == this) openPairing = nullptr;
		// Left without an answer (holding the power button goes home).
		if (state == State::Asking && confirm) phone::answerPairing(false);
		ui::keepAwake(false);
	}

	void build(lv_obj_t* root) override {
		this->root = root;
		lv_obj_t* content = theme::titled(root, tr(Str::Pairing));
		if (state == State::Done) {
			const bool ok = result == phone::PairResult::Ok;
			lv_obj_t* icon = theme::label(content, &font_large, ok ? theme::good() : theme::accent(),
				ok ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
			lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, ok ? 14 : 0);
			lv_obj_t* what = centered(content, &font_body, theme::text(), tr(ok ? Str::Paired : Str::PairFailed));
			lv_obj_align(what, LV_ALIGN_TOP_MID, 0, ok ? 44 : 22);
			if (!ok) {
				lv_obj_t* hint = centered(content, &font_small, theme::muted(), tr(Str::PairForgetOnPhone));
				lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 42);
			}
			return;
		}
		char digits[8];
		snprintf(digits, sizeof(digits), "%06lu", (unsigned long) (code % 1000000));
		lv_obj_t* number = theme::label(content, &font_large, theme::text(), digits);
		lv_obj_set_style_text_letter_space(number, 3, 0);
		lv_obj_align(number, LV_ALIGN_TOP_MID, 0, 6);

		const char* hint = state == State::Waiting ? tr(Str::PairWaiting)
			: confirm                                ? tr(Str::PairConfirm)
													 : tr(Str::PairEnter);
		lv_obj_t* h = centered(content, &font_small, theme::text(), hint);
		lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 36);
	}

	hints::Set controls() const override {
		if (state == State::Done) return { .back = LV_SYMBOL_LEFT };
		if (state == State::Asking && confirm) return { .press = { LV_SYMBOL_OK, theme::good() }, .back = LV_SYMBOL_CLOSE };
		return { .back = LV_SYMBOL_CLOSE };
	}

	uint32_t tickMs() const override { return 500; }

	void tick() override {
		const uint32_t shownFor = result == phone::PairResult::Ok ? 2500 : 8000;
		if (state == State::Done && nowMs() - doneAt > shownFor) ui::pop();
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action != ButtonAction::Click) return;
		if (state == State::Asking && confirm && (e.button == Button::Select || e.button == Button::Alt)) {
			const bool yes = e.button == Button::Select;
			phone::answerPairing(yes);
			state = State::Waiting;
			if (!yes) {
				ui::pop();
				return;
			}
			rebuild();
			return;
		}
		if (e.button == Button::Alt || state == State::Done) ui::pop();
	}

	void finish(phone::PairResult r) {
		state = State::Done;
		result = r;
		doneAt = nowMs();
		rebuild();
	}

private:
	static lv_obj_t* centered(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text) {
		lv_obj_t* l = theme::label(parent, font, color, text);
		lv_obj_set_width(l, 120);
		lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
		return l;
	}

	void rebuild() {
		if (!root) return;
		lv_obj_clean(root);
		build(root);
	}

	uint32_t code = 0;
	bool confirm = false;
	State state = State::Asking;
	phone::PairResult result = phone::PairResult::Ok;
	uint32_t doneAt = 0;
	lv_obj_t* root = nullptr;
};

}  // namespace

std::unique_ptr<Screen> makePairing(uint32_t code, bool confirm) {
	return std::make_unique<Pairing>(code, confirm);
}

std::unique_ptr<Screen> makePairingResult(phone::PairResult result) {
	return std::make_unique<Pairing>(result);
}

bool pairingFinished(phone::PairResult result) {
	if (!openPairing) return false;
	openPairing->finish(result);
	return true;
}

bool pairingScreenOpen() {
	return openPairing != nullptr;
}
