#pragma once

#include "drivers/buttons.h"
#include "hints.h"

#include <cstdint>
#include <functional>
#include <lvgl.h>
#include <memory>

class Display;
class Backlight;
class Buzzer;
class Buttons;
class Battery;
class Settings;

// A full-screen page. The UI task owns all screens; every method runs with
// the LVGL lock held.
class Screen {
public:
	virtual ~Screen() = default;
	// Create widgets on `root` (a fresh LVGL screen). Called again after a
	// language change, so keep no LVGL pointers across rebuilds except members
	// assigned here.
	virtual void build(lv_obj_t* root) = 0;
	virtual void onButton(const ButtonEvent& e) {}
	// What the wheel and the power button do right now, shown next to them
	// (ui/hints.h). Asked again after every button, tick and rebuild.
	virtual hints::Set controls() const { return {}; }
	// Called every tickMs() while the display is on, and once right after wake.
	virtual void tick() {}
	virtual uint32_t tickMs() const { return 1000; }
	// Transient screens (notification popups) close when the screen turns off.
	virtual bool transient() const { return false; }
};

namespace ui {

struct Devices {
	Display& display;
	Backlight& backlight;
	Buzzer& buzzer;
	Buttons& buttons;
	Battery& battery;
	Settings& settings;
};

void start(Devices& devices, std::unique_ptr<Screen> home);
Devices& devices();

void push(std::unique_ptr<Screen> screen);
void pop();
void popToHome();  // also: hold the power button anywhere
// Rebuild every screen (language or layout changed). Takes effect immediately
// for the visible one and lazily for the rest.
void rebuildAll();
// Settings > Flip screen: turns the picture 180°. The hints move to where
// the controls then are, and rolling the wheel toward the picture's left
// stays Button::Up.
void setFlipped(bool flipped);

void wake();
bool isScreenOn();
void sleep();  // screen off now, like the stock "lock"
void keepAwake(bool on);  // nests: each keepAwake(true) needs a keepAwake(false)

// Runs fn on the UI task with the LVGL lock held, within a few ms. Safe to
// call from any task; this is how BLE callbacks hand data to the UI.
void post(std::function<void()> fn);

// LVGL is not thread-safe. Anything outside the UI task that touches LVGL
// (console screenshots, BLE callbacks) must hold this lock.
bool lock(uint32_t timeoutMs = 1000);
void unlock();

}  // namespace ui
