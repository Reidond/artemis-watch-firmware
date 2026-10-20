#include "backlight.h"
#include "board/pins.h"

#include <algorithm>
#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr ledc_mode_t Mode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t Timer = LEDC_TIMER_1;
constexpr ledc_channel_t Channel = LEDC_CHANNEL_1;
constexpr ledc_timer_bit_t Resolution = LEDC_TIMER_10_BIT;
constexpr uint32_t FullDuty = (1u << Resolution) - 1;
constexpr uint32_t MinPercentDuty = 10;  // stock firmware's floor, keeps the panel readable
}  // namespace

void Backlight::init() {
	const ledc_timer_config_t timer = {
		.speed_mode = Mode,
		.duty_resolution = Resolution,
		.timer_num = Timer,
		.freq_hz = 5000,
		.clk_cfg = LEDC_AUTO_CLK,
		.deconfigure = false,
	};
	ESP_ERROR_CHECK(ledc_timer_config(&timer));
	const ledc_channel_config_t channel = {
		.gpio_num = pins::Backlight,
		.speed_mode = Mode,
		.channel = Channel,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = Timer,
		.duty = 0,
		.hpoint = 0,
		.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
		.flags = { .output_invert = 1 },
	};
	ESP_ERROR_CHECK(ledc_channel_config(&channel));
	current = 0;
}

uint32_t Backlight::duty(uint8_t percent) {
	if (percent == 0) return 0;
	percent = std::min<uint8_t>(percent, 100);
	const float p = percent / 100.0f;
	const float mapped = MinPercentDuty + (100 - MinPercentDuty) * p * p;  // gamma 2
	return uint32_t(mapped / 100.0f * FullDuty);
}

void Backlight::set(uint8_t percent) {
	ledc_set_duty(Mode, Channel, duty(percent));
	ledc_update_duty(Mode, Channel);
	current = percent;
}

void Backlight::fadeTo(uint8_t percent, uint32_t durationMs) {
	const int from = current;
	const int to = percent;
	constexpr int Steps = 25;
	for (int i = 1; i <= Steps; i++) {
		const int value = from + (to - from) * i / Steps;
		ledc_set_duty(Mode, Channel, duty(value));
		ledc_update_duty(Mode, Channel);
		vTaskDelay(pdMS_TO_TICKS(std::max<uint32_t>(1, durationMs / Steps)));
	}
	current = percent;
}

void Backlight::off() {
	set(0);
}
