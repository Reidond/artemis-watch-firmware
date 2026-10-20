#include "battery.h"
#include "board/pins.h"

#include <algorithm>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "battery";

namespace {
constexpr adc_atten_t Atten = ADC_ATTEN_DB_2_5;
constexpr float Divider = 4.0f;
constexpr float ReadOffsetMv = 75.0f;   // stock BattReadOffset
constexpr float RefExpectedMv = 2500.0f;
constexpr float EmaAlpha = 0.2f;
constexpr uint32_t SampleMs = 500;
}  // namespace

void Battery::init() {
	adc_unit_t unit;
	adc_channel_t chan;
	ESP_ERROR_CHECK(adc_oneshot_io_to_channel(pins::BattRead, &unit, &chan));
	channel = chan;

	adc_oneshot_unit_handle_t handle;
	const adc_oneshot_unit_init_cfg_t unitCfg = { .unit_id = unit, .clk_src = ADC_RTC_CLK_SRC_DEFAULT, .ulp_mode = ADC_ULP_MODE_DISABLE };
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&unitCfg, &handle));
	const adc_oneshot_chan_cfg_t chanCfg = { .atten = Atten, .bitwidth = ADC_BITWIDTH_12 };
	ESP_ERROR_CHECK(adc_oneshot_config_channel(handle, chan, &chanCfg));
	adc = handle;

	adc_cali_handle_t caliHandle;
	const adc_cali_curve_fitting_config_t caliCfg = { .unit_id = unit, .chan = chan, .atten = Atten, .bitwidth = ADC_BITWIDTH_12 };
	ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&caliCfg, &caliHandle));
	cali = caliHandle;

	gpio_config_t in = {
		.pin_bit_mask = (1ULL << pins::UsbDetect) | (1ULL << pins::ChargeIn) | (1ULL << pins::ChargeOut),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&in));
	gpio_set_level(pins::BattRefSwitch, 0);
	gpio_set_direction(pins::BattRefSwitch, GPIO_MODE_OUTPUT);

	calibrate();
	readCharge();
	sample(true);
	ESP_LOGI(TAG, "%d mV (%d%%), charge state %d, cal offset %.0f mV", mv.load(), percent(), int(chargeState.load()), calOffset);
	xTaskCreate(task, "battery", 3072, this, 4, nullptr);
}

float Battery::readPinMv() {
	int raw = 0, pinMv = 0;
	auto h = static_cast<adc_oneshot_unit_handle_t>(adc);
	if (adc_oneshot_read(h, adc_channel_t(channel), &raw) != ESP_OK) return -1;
	adc_cali_raw_to_voltage(static_cast<adc_cali_handle_t>(cali), raw, &pinMv);
	return float(pinMv);
}

void Battery::calibrate() {
	gpio_set_level(pins::BattRefSwitch, 1);
	vTaskDelay(pdMS_TO_TICKS(100));
	for (int i = 0; i < 10; i++) {
		readPinMv();
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	float total = 0;
	for (int i = 0; i < 10; i++) {
		total += readPinMv() * Divider;
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	gpio_set_level(pins::BattRefSwitch, 0);
	vTaskDelay(pdMS_TO_TICKS(100));
	const float reading = total / 10;
	calOffset = RefExpectedMv - reading;
	ESP_LOGI(TAG, "reference read %.0f mV, expected %.0f mV", reading, RefExpectedMv);
}

void Battery::sample(bool fresh) {
	const float pin = readPinMv();
	if (pin < 0) return;
	const float v = pin * Divider + ReadOffsetMv + calOffset;
	ema = (fresh || ema < 0) ? v : ema + EmaAlpha * (v - ema);
	mv.store(int(ema));
	if (chargeState.load() == Charge::Unplugged) unpluggedMv.store(int(ema));
}

void Battery::readCharge() {
	const bool plugged = gpio_get_level(pins::UsbDetect) == 1;
	const bool charging = gpio_get_level(pins::ChargeIn) == 0;
	chargeState.store(!plugged ? Charge::Unplugged : charging ? Charge::Charging : Charge::Full);
}

int Battery::percent() const {
	const int v = unpluggedMv.load();
	if (v < 0) return -1;
	if (chargeState.load() == Charge::Full) return 100;
	return std::clamp((v - EmptyMv) * 100 / (FullMv - EmptyMv), 0, 100);
}

void Battery::task(void* arg) {
	auto* self = static_cast<Battery*>(arg);
	while (true) {
		vTaskDelay(pdMS_TO_TICKS(SampleMs));
		const auto before = self->charge();
		self->readCharge();
		// Plugging/unplugging shifts the voltage a lot; restart the average.
		self->sample(before != self->charge());
	}
}
