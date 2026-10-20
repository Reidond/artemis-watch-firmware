// Orbit watch face (design/concepts/02-watchface-orbit.png): mission day
// counter, time, and an Earth-Moon trajectory. Earth shows the real day/night
// line, the Moon its real phase, and the red craft flies the loop once a day:
// at Earth at midnight, past the Moon at noon.
#include "screens.h"
#include "ui/art/earth.h"
#include "ui/art/moon.h"
#include "ui/dither.h"
#include "ui/fonts/icons.h"
#include "ui/theme.h"

#include "drivers/battery.h"
#include "i18n/i18n.h"
#include "services/clock.h"
#include "services/mission.h"
#include "services/phone.h"
#include "services/weather.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr int CanvasW = 128, CanvasH = 116;
constexpr int EarthX = 31, EarthY = 88, EarthR = 20;
constexpr int MoonX = 108, MoonY = 58, MoonR = 9;
// Trajectory: a tilted ellipse around Earth's far side and the Moon.
constexpr float OrbitX = 66, OrbitY = 73, OrbitA = 60, OrbitB = 20, OrbitTilt = -0.33f;
// Earth is seen from above Kyiv's longitude, tilted to show the north.
constexpr float ViewLat = 25, ViewLon = 30.5f;
// Shown while the clock is unset: noon, so Earth is lit.
constexpr time_t PlaceholderTime = 1767268800;

const char* batteryIcon(int pct, Charge c) {
	if (c != Charge::Unplugged) return LV_SYMBOL_CHARGE;
	if (pct > 80) return LV_SYMBOL_BATTERY_FULL;
	if (pct > 55) return LV_SYMBOL_BATTERY_3;
	if (pct > 30) return LV_SYMBOL_BATTERY_2;
	if (pct > 10) return LV_SYMBOL_BATTERY_1;
	return LV_SYMBOL_BATTERY_EMPTY;
}

void orbitPoint(float theta, float& x, float& y) {
	const float ct = std::cos(theta), st = std::sin(theta);
	const float cp = std::cos(OrbitTilt), sp = std::sin(OrbitTilt);
	x = OrbitX + OrbitA * ct * cp - OrbitB * st * sp;
	y = OrbitY + OrbitA * ct * sp + OrbitB * st * cp;
}

class OrbitFace : public Screen {
public:
	void build(lv_obj_t* root) override {
		lv_obj_t* art = canvas.create(root, CanvasW, CanvasH);
		lv_obj_set_pos(art, 0, 0);
		drawnMinute = -2;

		mission = theme::label(root, &font_label, theme::accent());
		lv_obj_set_pos(mission, 4, 1);

		date = theme::label(root, &font_small, theme::text());
		lv_obj_align(date, LV_ALIGN_TOP_RIGHT, -3, 1);

		time = theme::label(root, &font_clock, theme::text());
		lv_obj_align(time, LV_ALIGN_TOP_MID, 0, 15);

		ampm = theme::label(root, &font_small, theme::muted());
		lv_obj_align(ampm, LV_ALIGN_TOP_RIGHT, -3, 20);

		status = theme::label(root, &font_small, theme::text());
		lv_obj_align(status, LV_ALIGN_BOTTOM_RIGHT, -3, -1);
	}

	void tick() override {
		showStatus();
		if (!clock_svc::isValid()) {
			lv_label_set_text(time, "--:--");
			lv_label_set_text(date, "");
			lv_label_set_text(mission, "T+---");
			lv_label_set_text(ampm, "");
			if (drawnMinute != -1) drawArt(PlaceholderTime, -1);
			return;
		}

		const time_t now = ::time(nullptr);
		const tm t = clock_svc::local();
		char buf[32];
		const bool h24 = ui::devices().settings.get().use24h;
		const int hour = h24 ? t.tm_hour : (t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12);
		snprintf(buf, sizeof(buf), h24 ? "%02d:%02d" : "%d:%02d", hour, t.tm_min);
		lv_label_set_text(time, buf);
		lv_label_set_text(ampm, h24 ? "" : tr(t.tm_hour < 12 ? Str::Am : Str::Pm));

		snprintf(buf, sizeof(buf), "%d %s", t.tm_mday, i18n::monthGenitive(t.tm_mon));
		lv_label_set_text(date, buf);

		const int day = mission::day(now);
		snprintf(buf, sizeof(buf), day >= 0 ? "T+%03d" : "T-%03d", day >= 0 ? day : -day);
		lv_label_set_text(mission, buf);

		const int minuteOfDay = t.tm_hour * 60 + t.tm_min;
		if (minuteOfDay != drawnMinute) drawArt(now, minuteOfDay);
	}

	void onButton(const ButtonEvent& e) override {
		if (e.action != ButtonAction::Click) return;
		switch (e.button) {
			case Button::Select: ui::push(makeMenu()); break;
			case Button::Up: ui::push(makeNotifications()); break;
			case Button::Down: ui::push(makeWeather()); break;
			case Button::Alt: ui::sleep(); break;
		}
	}

	hints::Set controls() const override {
		return { LV_SYMBOL_BELL, ICON_CLOUD, LV_SYMBOL_LIST, ICON_LOCK };
	}

private:
	// Redrawn once a minute: the terminator and the craft move slowly.
	// minuteOfDay < 0: clock unset, no craft.
	void drawArt(time_t now, int minuteOfDay) {
		drawnMinute = minuteOfDay;
		canvas.clear(theme::bg());
		art::drawEarth(canvas, EarthX, EarthY, EarthR, now, ViewLat, ViewLon, dither::grey());
		drawOrbit();
		art::drawMoon(canvas, MoonX, MoonY, MoonR, minuteOfDay < 0 ? 0.5f : art::moonPhase(now), dither::grey());
		if (minuteOfDay >= 0) drawCraft(minuteOfDay / 1440.0f);
		canvas.commit();
	}

	// Dashed line; the half behind Earth is hidden by it.
	void drawOrbit() {
		const lv_color_t line = theme::text();
		float travelled = 0, px = 0, py = 0;
		orbitPoint(0, px, py);
		for (int i = 1; i <= 720; i++) {
			const float theta = i * 2.0f * float(M_PI) / 720;
			float x, y;
			orbitPoint(theta, x, y);
			travelled += std::hypot(x - px, y - py);
			px = x;
			py = y;
			if (std::fmod(travelled, 5.0f) >= 3.0f) continue;  // 3 on, 2 off
			const float dx = x - EarthX, dy = y - EarthY;
			const bool behindEarth = std::sin(theta) < 0 && dx * dx + dy * dy < float(EarthR * EarthR);
			if (!behindEarth) canvas.set(int(x), int(y), line);
		}
	}

	// Red craft: leaves Earth at midnight over the upper arc, passes the Moon at noon.
	void drawCraft(float dayFraction) {
		float x, y;
		orbitPoint(float(M_PI) + dayFraction * 2.0f * float(M_PI), x, y);
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) canvas.set(int(x) + dx, int(y) + dy, theme::accent());
		}
	}

	void showStatus() {
		auto& dev = ui::devices();
		int count = 0;
		for (const auto& n : phone::notifications()) {
			if (n.category != phone::Category::IncomingCall) count++;
		}
		const auto link = phone::link();
		const int pct = dev.battery.percent();
		const Charge charge = dev.battery.charge();

		char buf[64] = "";
		int len = 0;
		if (const auto* w = weather::current()) len += snprintf(buf, sizeof(buf), "%ld° ", std::lround(w->temp));
		if (count) len += snprintf(buf + len, sizeof(buf) - len, LV_SYMBOL_BELL "%d ", count);
		if (link != phone::Link::Off) len += snprintf(buf + len, sizeof(buf) - len, "%s ", LV_SYMBOL_BLUETOOTH);
		if (pct < 0) snprintf(buf + len, sizeof(buf) - len, "%s", batteryIcon(100, charge));
		else snprintf(buf + len, sizeof(buf) - len, "%d%%", pct);
		lv_label_set_text(status, buf);
		const bool low = pct >= 0 && pct <= 15 && charge == Charge::Unplugged;
		// White while the phone is fully connected, grey otherwise; red on low battery.
		lv_obj_set_style_text_color(status, low ? theme::accent() : link == phone::Link::Ready ? theme::text() : theme::muted(), 0);
	}

	dither::Canvas canvas;
	int drawnMinute = -2;
	lv_obj_t* mission = nullptr;
	lv_obj_t* date = nullptr;
	lv_obj_t* time = nullptr;
	lv_obj_t* ampm = nullptr;
	lv_obj_t* status = nullptr;
};

}  // namespace

std::unique_ptr<Screen> makeWatchFace() {
	return std::make_unique<OrbitFace>();
}
