#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <lvgl.h>

#include "i18n/i18n.h"

// Weather pushed by the companion app (docs/BLE_PROTOCOL.md, Weather).
// Owned by the UI task: call set() through ui::post.
namespace weather {

struct Hour {
	int8_t hour;   // local hour 0..23
	int8_t temp;   // °C
	uint8_t code;  // WMO
};

struct Data {
	int64_t ts = 0;  // when the app fetched it (unix)
	char place[48] = "";
	float temp = 0;
	uint8_t code = 0;
	float hi = 0, lo = 0;
	uint8_t hourCount = 0;
	Hour hours[6] = {};
};

// Parses the app's JSON; false if it is not a weather object.
bool parse(const char* json, size_t len, Data& out);

void load();  // from NVS, at boot
void set(const Data& data);
// Latest data, or nullptr when there is none or it is older than 12 hours.
const Data* current();
int64_t lastUpdate();  // unix seconds, 0 = never

Str condition(uint8_t code);
const lv_image_dsc_t* icon(uint8_t code, bool night);

}  // namespace weather
