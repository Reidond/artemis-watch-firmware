#include "screens.h"
#include "ui/list_screen.h"

#include "drivers/backlight.h"
#include "drivers/buzzer.h"
#include "i18n/i18n.h"
#include "services/phone.h"
#include "services/settings.h"

#include <cstdio>

namespace {

constexpr uint8_t BrightnessSteps[] = { 20, 40, 60, 80, 100 };

std::string onOff(bool v) {
	return tr(v ? Str::On : Str::Off);
}

std::string timeoutText(uint16_t seconds) {
	char buf[24];
	if (seconds < 60) snprintf(buf, sizeof(buf), "%u %s", seconds, tr(Str::Seconds));
	else snprintf(buf, sizeof(buf), "%u %s", seconds / 60, tr(Str::Minutes));
	return buf;
}

std::string linkText() {
	switch (phone::link()) {
		case phone::Link::Off: return tr(Str::LinkOff);
		case phone::Link::Ready: return tr(Str::LinkConnected);
		case phone::Link::Connected: return tr(Str::LinkConnecting);
		default: return tr(phone::bonded() ? Str::LinkSearching : Str::NotPaired);
	}
}

class SettingsScreen : public ListScreen {
public:
	void tick() override { refreshValues(); }

protected:
	std::string title() override { return tr(Str::MenuSettings); }

	std::vector<ListItem> items() override {
		auto& dev = ui::devices();
		auto& s = dev.settings;
		return {
			{ LV_SYMBOL_EDIT, [] { return tr(Str::SetLanguage); }, [] { return tr(Str::LanguageName); },
				[&s] {
					auto& d = s.get();
					d.language = d.language == Lang::Uk ? Lang::En : Lang::Uk;
					i18n::setLanguage(d.language);
					s.save();
					ui::rebuildAll();
				} },
			{ LV_SYMBOL_IMAGE, [] { return tr(Str::SetBrightness); },
				[&s] { return std::to_string(s.get().brightness) + "%"; },
				[&s, &dev] {
					auto& d = s.get();
					uint8_t next = BrightnessSteps[0];
					for (auto step : BrightnessSteps) {
						if (step > d.brightness) {
							next = step;
							break;
						}
					}
					d.brightness = next;
					dev.backlight.set(next);
					s.save();
				} },
			{ LV_SYMBOL_EYE_CLOSE, [] { return tr(Str::SetScreenTimeout); },
				[&s] { return timeoutText(Settings::ScreenTimeouts[s.get().screenTimeoutIdx]); },
				[&s] {
					auto& d = s.get();
					d.screenTimeoutIdx = (d.screenTimeoutIdx + 1) % Settings::ScreenTimeoutCount;
					s.save();
				} },
			{ LV_SYMBOL_REFRESH, [] { return tr(Str::SetTimeFormat); },
				[&s] { return tr(s.get().use24h ? Str::Hours24 : Str::Hours12); },
				[&s] {
					s.get().use24h = !s.get().use24h;
					s.save();
				} },
			{ LV_SYMBOL_LOOP, [] { return tr(Str::SetFlipScreen); }, [&s] { return onOff(s.get().flipped); },
				[&s] {
					s.get().flipped = !s.get().flipped;
					ui::setFlipped(s.get().flipped);
					s.save();
				} },
			{ LV_SYMBOL_VOLUME_MAX, [] { return tr(Str::SetSounds); }, [&s] { return onOff(s.get().sounds); },
				[&s, &dev] {
					s.get().sounds = !s.get().sounds;
					dev.buzzer.setEnabled(s.get().sounds);
					dev.buzzer.chime();
					s.save();
				} },
			{ LV_SYMBOL_BLUETOOTH, [] { return tr(Str::SetPhone); }, linkText,
				[] {
					if (phone::bonded()) ui::push(makeForgetPhone());
				} },
			{ LV_SYMBOL_POWER, [] { return tr(Str::SetPowerOff); }, nullptr, [] { ui::push(makePowerOff()); } },
		};
	}
};

}  // namespace

std::unique_ptr<Screen> makeSettings() {
	return std::make_unique<SettingsScreen>();
}
