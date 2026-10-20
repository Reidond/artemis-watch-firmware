#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

enum class Button : uint8_t { Up, Down, Select, Alt };

enum class ButtonAction : uint8_t {
	Press,   // went down
	Click,   // released before LongMs
	Long,    // held for LongMs (fires once, no Click follows)
	Repeat,  // keeps firing every RepeatMs after Long while held
};

struct ButtonEvent {
	Button button;
	ButtonAction action;
};

// Polls the four active-high buttons (external pull-downs, stock wiring),
// debounces them and posts ButtonEvents to a queue.
class Buttons {
public:
	static constexpr uint32_t LongMs = 600;
	static constexpr uint32_t RepeatMs = 150;

	void init();
	bool next(ButtonEvent& out, TickType_t wait);
	void inject(Button b, bool longPress);  // used by the debug console
	bool isDown(Button b) const;

private:
	static void task(void* arg);
	void post(Button b, ButtonAction a);

	QueueHandle_t queue = nullptr;
	struct State {
		bool down = false;
		uint8_t stable = 0;
		uint32_t downAt = 0;
		uint32_t lastRepeat = 0;
		bool longFired = false;
	} state[4];
};

const char* buttonName(Button b);
