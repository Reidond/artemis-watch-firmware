#include "buzzer.h"
#include "board/pins.h"

#include <driver/ledc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {
constexpr ledc_mode_t Mode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t Timer = LEDC_TIMER_0;
constexpr ledc_channel_t Channel = LEDC_CHANNEL_0;
constexpr ledc_timer_bit_t Resolution = LEDC_TIMER_10_BIT;
constexpr uint32_t HalfDuty = 1u << (Resolution - 1);
}  // namespace

void Buzzer::init() {
	const ledc_timer_config_t timer = {
		.speed_mode = Mode,
		.duty_resolution = Resolution,
		.timer_num = Timer,
		.freq_hz = 2000,
		.clk_cfg = LEDC_AUTO_CLK,
		.deconfigure = false,
	};
	ESP_ERROR_CHECK(ledc_timer_config(&timer));
	const ledc_channel_config_t channel = {
		.gpio_num = pins::Buzzer,
		.speed_mode = Mode,
		.channel = Channel,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = Timer,
		.duty = 0,
		.hpoint = 0,
		.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
		.flags = { .output_invert = 0 },
	};
	ESP_ERROR_CHECK(ledc_channel_config(&channel));
	queue = xQueueCreate(4, sizeof(Melody));
	xTaskCreate(task, "buzzer", 2048, this, 3, nullptr);
}

void Buzzer::play(std::initializer_list<Tone> tones, bool evenIfMuted) {
	if ((!enabled && !evenIfMuted) || !queue) return;
	Melody m{};
	for (const auto& t : tones) {
		if (m.count == MaxTones) break;
		m.tones[m.count++] = t;
	}
	xQueueSend(static_cast<QueueHandle_t>(queue), &m, 0);
}

void Buzzer::tone(uint16_t freq) {
	if (freq == 0) {
		ledc_set_duty(Mode, Channel, 0);
	} else {
		ledc_set_freq(Mode, Timer, freq);
		ledc_set_duty(Mode, Channel, HalfDuty);
	}
	ledc_update_duty(Mode, Channel);
}

void Buzzer::task(void* arg) {
	auto* self = static_cast<Buzzer*>(arg);
	Melody m;
	while (true) {
		xQueueReceive(static_cast<QueueHandle_t>(self->queue), &m, portMAX_DELAY);
		for (int i = 0; i < m.count; i++) {
			self->tone(m.tones[i].freq);
			vTaskDelay(pdMS_TO_TICKS(m.tones[i].ms));
		}
		self->tone(0);
	}
}
