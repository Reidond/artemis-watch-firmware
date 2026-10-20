#include "buttons.h"
#include "board/pins.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>

static const char* TAG = "buttons";

namespace {
constexpr gpio_num_t Pins[4] = { pins::BtnUp, pins::BtnDown, pins::BtnSelect, pins::BtnAlt };
constexpr uint32_t PollMs = 10;
constexpr uint8_t DebounceSamples = 2;

uint32_t nowMs() {
	return uint32_t(esp_timer_get_time() / 1000);
}
}  // namespace

const char* buttonName(Button b) {
	static const char* names[] = { "up", "down", "select", "alt" };
	return names[int(b)];
}

void Buttons::init() {
	uint64_t mask = 0;
	for (auto pin : Pins) mask |= 1ULL << pin;
	const gpio_config_t cfg = {
		.pin_bit_mask = mask,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&cfg));
	queue = xQueueCreate(16, sizeof(ButtonEvent));
	xTaskCreate(task, "buttons", 3072, this, 6, nullptr);
}

bool Buttons::next(ButtonEvent& out, TickType_t wait) {
	return xQueueReceive(queue, &out, wait) == pdTRUE;
}

bool Buttons::isDown(Button b) const {
	return state[int(b)].down;
}

void Buttons::post(Button b, ButtonAction a) {
	const ButtonEvent e{ b, a };
	xQueueSend(queue, &e, 0);
}

void Buttons::inject(Button b, bool longPress) {
	post(b, ButtonAction::Press);
	post(b, longPress ? ButtonAction::Long : ButtonAction::Click);
}

void Buttons::task(void* arg) {
	auto* self = static_cast<Buttons*>(arg);
	while (true) {
		const uint32_t now = nowMs();
		for (int i = 0; i < 4; i++) {
			auto& s = self->state[i];
			const bool raw = gpio_get_level(Pins[i]);
			if (raw != s.down) {
				if (++s.stable < DebounceSamples) continue;
				s.stable = 0;
				s.down = raw;
				const auto b = Button(i);
				if (s.down) {
					s.downAt = now;
					s.longFired = false;
					self->post(b, ButtonAction::Press);
					ESP_LOGI(TAG, "%s pressed", buttonName(b));
				} else if (!s.longFired) {
					self->post(b, ButtonAction::Click);
				}
				continue;
			}
			s.stable = 0;
			if (!s.down) continue;
			if (!s.longFired && now - s.downAt >= LongMs) {
				s.longFired = true;
				s.lastRepeat = now;
				self->post(Button(i), ButtonAction::Long);
			} else if (s.longFired && now - s.lastRepeat >= RepeatMs) {
				s.lastRepeat = now;
				self->post(Button(i), ButtonAction::Repeat);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(PollMs));
	}
}
