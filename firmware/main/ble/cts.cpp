// Current Time Service client (Bluetooth SIG CTS 1.1): Current Time (Exact
// Time 256, the phone's local time) plus Local Time Information (time zone and
// DST offset in 15-minute steps), turned into UTC on the UI task.
#include "apple.h"

#include "services/phone.h"
#include "ui/ui.h"

#include <ctime>
#include <esp_log.h>

static const char* TAG = "cts";

namespace ble::cts {
namespace {

const ble_uuid16_t Service = BLE_UUID16_INIT(0x1805);
const ble_uuid16_t CurrentTime = BLE_UUID16_INIT(0x2A2B);
const ble_uuid16_t LocalTimeInfo = BLE_UUID16_INIT(0x2A0F);

uint16_t currentTime = 0;
uint16_t localTimeInfo = 0;
bool ready = false;

void apply(const std::vector<uint8_t>& ct, bool offsetKnown, int offset) {
	if (ct.size() < 7) return;
	const int year = ct[0] | ct[1] << 8;
	if (year < 2024 || ct[2] < 1 || ct[2] > 12 || ct[3] < 1 || ct[3] > 31) {
		ESP_LOGW(TAG, "implausible time %d-%u-%u", year, ct[2], ct[3]);
		return;
	}
	tm local = {};
	local.tm_year = year - 1900;
	local.tm_mon = ct[2] - 1;
	local.tm_mday = ct[3];
	local.tm_hour = ct[4];
	local.tm_min = ct[5];
	local.tm_sec = ct[6];
	ESP_LOGI(TAG, "%04d-%02u-%02u %02u:%02u:%02u, offset %s%d s", year, ct[2], ct[3], ct[4], ct[5], ct[6],
		offsetKnown ? "" : "unknown ", offset);
	ui::post([local, offsetKnown, offset] { phone::feed::time(local, offsetKnown, offset); });
}

void readTime(bool offsetKnown, int offset) {
	gatt::read(currentTime, [offsetKnown, offset](int status, const std::vector<uint8_t>& v) {
		if (status != 0) {
			ESP_LOGW(TAG, "read current time: 0x%x", status);
			return;
		}
		apply(v, offsetKnown, offset);
	});
}

// Reads the offset first so the time is converted with the zone it was given in.
void sync() {
	if (!localTimeInfo) {
		readTime(false, 0);
		return;
	}
	gatt::read(localTimeInfo, [](int status, const std::vector<uint8_t>& v) {
		bool known = false;
		int offset = 0;
		if (status == 0 && v.size() >= 2 && int8_t(v[0]) != -128) {
			known = true;
			offset = int8_t(v[0]) * 900 + (v[1] != 255 ? v[1] * 900 : 0);
		}
		readTime(known, offset);
	});
}

}  // namespace

const ble_uuid_t* service() {
	return &Service.u;
}

void attach(const gatt::Service& svc) {
	const auto* ct = svc.find(&CurrentTime.u);
	const auto* lti = svc.find(&LocalTimeInfo.u);
	if (!ct) {
		ESP_LOGW(TAG, "no current time characteristic");
		return;
	}
	currentTime = ct->value;
	localTimeInfo = lti ? lti->value : 0;
	ready = true;
	if (ct->cccd) {
		// The phone notifies on manual changes, time zone and DST switches.
		gatt::listen(ct->value, [](const uint8_t*, size_t) { sync(); });
		gatt::subscribe(ct->cccd, 1);
	}
	sync();
}

void detach() {
	ready = false;
	currentTime = localTimeInfo = 0;
}

bool attached() {
	return ready;
}

}  // namespace ble::cts
