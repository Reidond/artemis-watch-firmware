#include "board.h"
#include "pins.h"

#include <esp_efuse.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "board";

namespace {

constexpr esp_efuse_desc_t PidField = { EFUSE_BLK3, 16, 16 };
constexpr esp_efuse_desc_t RevField = { EFUSE_BLK3, 32, 8 };
const esp_efuse_desc_t* PidDesc[] = { &PidField, nullptr };
const esp_efuse_desc_t* RevDesc[] = { &RevField, nullptr };

void setOutput(gpio_num_t pin, int level) {
	gpio_set_level(pin, level);
	const gpio_config_t cfg = {
		.pin_bit_mask = 1ULL << pin,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	gpio_config(&cfg);
	gpio_set_level(pin, level);
}

i2c_master_bus_handle_t bus = nullptr;

}  // namespace

namespace board {

Identity identity() {
	Identity id;
	esp_efuse_read_field_blob(PidDesc, &id.pid, 16);
	esp_efuse_read_field_blob(RevDesc, &id.revision, 8);
	return id;
}

bool supported(const Identity& id) {
	return id.pid == 0x0003 && (id.revision == 2 || id.revision == 3);
}

void initSafeState() {
	setOutput(pins::Backlight, 1);
	for (auto led : pins::Leds) {
		setOutput(led, 0);
	}
	setOutput(pins::RgbR, 1);
	setOutput(pins::RgbG, 1);
	setOutput(pins::RgbB, 1);
	setOutput(pins::Buzzer, 0);
}

i2c_master_bus_handle_t i2c() {
	if (bus) return bus;
	const i2c_master_bus_config_t cfg = {
		.i2c_port = I2C_NUM_0,
		.sda_io_num = pins::I2cSda,
		.scl_io_num = pins::I2cScl,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.intr_priority = 0,
		.trans_queue_depth = 0,
		.flags = { .enable_internal_pullup = true, .allow_pd = false },
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
	return bus;
}

void powerOff() {
	ESP_LOGW(TAG, "power off");
	setOutput(pins::Backlight, 1);
	vTaskDelay(pdMS_TO_TICKS(50));
	setOutput(pins::PowerDown, 0);
	// Still running: USB keeps the watch powered.
	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_restart();
}

}  // namespace board
