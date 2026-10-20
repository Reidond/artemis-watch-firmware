#include "ble/ble.h"
#include "board/board.h"
#include "debug/console.h"
#include "drivers/backlight.h"
#include "drivers/battery.h"
#include "drivers/buttons.h"
#include "drivers/buzzer.h"
#include "drivers/display.h"
#include "drivers/rtc.h"
#include "i18n/i18n.h"
#include "services/clock.h"
#include "services/settings.h"
#include "services/weather.h"
#include "ui/screens/screens.h"
#include "ui/ui.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

static const char* TAG = "main";

namespace {

Display display;
Backlight backlight;
Buzzer buzzer;
Buttons buttons;
Battery battery;
Rtc rtc;
Settings settings;

void initNvs() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
}

}  // namespace

extern "C" void app_main() {
	board::initSafeState();

	const auto id = board::identity();
	ESP_LOGI(TAG, "PID 0x%04x, HW rev %u", id.pid, id.revision);
	if (!board::supported(id)) {
		ESP_LOGE(TAG, "unsupported hardware, halting");
		while (true) vTaskDelay(portMAX_DELAY);
	}

	initNvs();
	settings.load();
	weather::load();
	i18n::setLanguage(settings.get().language);

	backlight.init();
	display.init(settings.get().flipped);
	buzzer.init();
	buzzer.setEnabled(settings.get().sounds);
	buttons.init();
	battery.init();
	if (!rtc.init(board::i2c())) ESP_LOGE(TAG, "RTC init failed");
	clock_svc::init(rtc, settings.get().timezone);

	static ui::Devices devices{ display, backlight, buzzer, buttons, battery, settings };
	installPhoneEvents();
	ui::start(devices, makeWatchFace());
	buzzer.chime();

	ble::start();
	console::start();
}
