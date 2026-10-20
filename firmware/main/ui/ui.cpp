#include "ui.h"
#include "theme.h"

#include "drivers/backlight.h"
#include "drivers/battery.h"
#include "drivers/buzzer.h"
#include "drivers/display.h"
#include "services/settings.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mutex>
#include <vector>

static const char* TAG = "ui";

namespace {

constexpr int BufferRows = 32;

struct Entry {
	std::unique_ptr<Screen> screen;
	lv_obj_t* root = nullptr;
	uint32_t builtEpoch = 0;
	uint32_t lastTick = 0;
	hints::Set shownHints;
};

ui::Devices* dev = nullptr;
SemaphoreHandle_t mutex = nullptr;
lv_display_t* lvDisplay = nullptr;
std::vector<Entry> stack;

// Navigation requested from inside handlers is applied afterwards, so a
// screen never gets destroyed while one of its own methods is running.
std::vector<std::unique_ptr<Screen>> pendingPush;
int pendingPops = 0;
bool pendingHome = false;

// Work handed over by other tasks (ui::post). Guarded by its own mutex so a
// poster never waits for a frame to render.
std::mutex postedMutex;
std::vector<std::function<void()>> posted;

uint32_t epoch = 1;
bool flipped = false;
bool screenOn = true;
int stayAwake = 0;  // keepAwake(true) calls not yet matched by keepAwake(false)
volatile bool wakeRequested = false;
bool sleepRequested = false;
uint32_t lastActivity = 0;
int swallow = -1;  // button whose events are ignored after it woke the screen

uint32_t nowMs() {
	return uint32_t(esp_timer_get_time() / 1000);
}

void flushCb(lv_display_t* disp, const lv_area_t* a, uint8_t* px) {
	const uint32_t count = lv_area_get_size(a);
	lv_draw_sw_rgb565_swap(px, count);
	dev->display.draw(a->x1, a->y1, a->x2, a->y2, px, count * 2);
}

void flushDone(void* ctx) {
	lv_display_flush_ready(static_cast<lv_display_t*>(ctx));
}

void refreshHints(Entry& e) {
	if (e.root) hints::apply(e.root, e.screen->controls(), e.shownHints);
}

void build(Entry& e) {
	lv_obj_t* old = e.root;
	e.root = theme::screen();
	e.screen->build(e.root);
	e.builtEpoch = epoch;
	e.screen->tick();
	e.lastTick = nowMs();
	refreshHints(e);
	if (old) {
		if (lv_screen_active() == old) lv_screen_load(e.root);
		lv_obj_delete(old);
	}
}

void show(Entry& e) {
	if (!e.root || e.builtEpoch != epoch) build(e);
	lv_screen_load(e.root);
	e.screen->tick();
	e.lastTick = nowMs();
	refreshHints(e);
}

void popTop() {
	if (stack.size() <= 1) return;
	lv_obj_t* old = stack.back().root;
	show(stack[stack.size() - 2]);
	if (old) lv_obj_delete(old);
	stack.pop_back();
}

void applyPending() {
	if (pendingHome) {
		pendingHome = false;
		while (stack.size() > 1) popTop();
	}
	for (; pendingPops > 0; pendingPops--) popTop();
	for (auto& s : pendingPush) {
		stack.push_back(Entry{ std::move(s) });
		show(stack.back());
	}
	pendingPush.clear();
}

void screenOff() {
	dev->backlight.fadeTo(0, 150);
	dev->display.setSleep(true);
	screenOn = false;
	while (stack.size() > 1 && stack.back().screen->transient()) popTop();
	ESP_LOGD(TAG, "screen off");
}

void runPosted() {
	std::vector<std::function<void()>> work;
	{
		std::lock_guard<std::mutex> guard(postedMutex);
		work.swap(posted);
	}
	for (auto& fn : work) fn();
}

void screenOnNow() {
	dev->display.setSleep(false);
	auto& top = stack.back();
	if (top.builtEpoch != epoch) build(top);
	top.screen->tick();
	top.lastTick = nowMs();
	refreshHints(top);
	lv_obj_invalidate(lv_screen_active());
	lv_refr_now(lvDisplay);
	dev->backlight.fadeTo(dev->settings.get().brightness, 120);
	screenOn = true;
	lastActivity = nowMs();
}

void handleButton(ButtonEvent e) {
	// Flipped, the wheel rolls the other way relative to the picture.
	if (flipped && e.button == Button::Up) e.button = Button::Down;
	else if (flipped && e.button == Button::Down) e.button = Button::Up;
	if (!screenOn) {
		if (e.action == ButtonAction::Press) {
			screenOnNow();
			swallow = int(e.button);
		}
		return;
	}
	if (swallow == int(e.button)) {
		if (e.action != ButtonAction::Press) return;
		swallow = -1;
	}
	lastActivity = nowMs();
	// Holding the power button goes back to the watch face from anywhere.
	if (e.button == Button::Alt && e.action == ButtonAction::Long && stack.size() > 1) ui::popToHome();
	else stack.back().screen->onButton(e);
	applyPending();
}

void task(void*) {
	lastActivity = nowMs();
	while (true) {
		ButtonEvent e;
		const bool got = dev->buttons.next(e, pdMS_TO_TICKS(screenOn ? 5 : 50));
		ui::lock(portMAX_DELAY);
		runPosted();
		applyPending();
		// Wake requests first, so a button press queued right after one is
		// handled as a normal press instead of being swallowed as the wake press.
		if (wakeRequested) {
			wakeRequested = false;
			if (!screenOn) screenOnNow();
			lastActivity = nowMs();
		}
		if (got) handleButton(e);
		if (sleepRequested) {
			sleepRequested = false;
			if (screenOn) screenOff();
		}
		if (screenOn) {
			auto& top = stack.back();
			if (top.builtEpoch != epoch) build(top);
			const uint32_t now = nowMs();
			if (now - top.lastTick >= top.screen->tickMs()) {
				top.lastTick = now;
				top.screen->tick();
				applyPending();
			}
			refreshHints(stack.back());
			lv_timer_handler();
			if (!stayAwake && now - lastActivity > dev->settings.screenTimeoutMs()) screenOff();
		}
		ui::unlock();
	}
}

}  // namespace

namespace ui {

void start(Devices& devices, std::unique_ptr<Screen> home) {
	dev = &devices;
	mutex = xSemaphoreCreateRecursiveMutex();

	lv_init();
	lv_tick_set_cb(nowMs);
	lvDisplay = lv_display_create(Display::Width, Display::Height);
	lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);
	const size_t bytes = Display::Width * BufferRows * 2;
	void* buf1 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	void* buf2 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	lv_display_set_buffers(lvDisplay, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(lvDisplay, flushCb);
	dev->display.setDoneCallback(flushDone, lvDisplay);

	lv_theme_t* th = lv_theme_default_init(lvDisplay, theme::muted(), theme::accent(), true, &font_body);
	lv_display_set_theme(lvDisplay, th);

	flipped = dev->settings.get().flipped;
	hints::setFlipped(flipped);
	stack.push_back(Entry{ std::move(home) });
	show(stack.back());
	lv_refr_now(lvDisplay);
	dev->backlight.fadeTo(dev->settings.get().brightness, 200);

	xTaskCreatePinnedToCore(task, "ui", 8192, nullptr, 5, nullptr, 1);
}

Devices& devices() {
	return *dev;
}

void push(std::unique_ptr<Screen> screen) {
	pendingPush.push_back(std::move(screen));
}

void pop() {
	pendingPops++;
}

void popToHome() {
	pendingHome = true;
}

void rebuildAll() {
	epoch++;
}

void setFlipped(bool on) {
	flipped = on;
	dev->display.setFlipped(on);
	hints::setFlipped(on);
	rebuildAll();
}

bool isScreenOn() {
	return screenOn;
}

void wake() {
	wakeRequested = true;
}

void sleep() {
	sleepRequested = true;
}

void keepAwake(bool on) {
	stayAwake = on ? stayAwake + 1 : (stayAwake > 0 ? stayAwake - 1 : 0);
	lastActivity = nowMs();
}

void post(std::function<void()> fn) {
	std::lock_guard<std::mutex> guard(postedMutex);
	posted.push_back(std::move(fn));
}

bool lock(uint32_t timeoutMs) {
	const TickType_t ticks = timeoutMs == portMAX_DELAY ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
	return xSemaphoreTakeRecursive(mutex, ticks) == pdTRUE;
}

void unlock() {
	xSemaphoreGiveRecursive(mutex);
}

}  // namespace ui
