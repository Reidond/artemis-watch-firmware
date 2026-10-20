#include "clock.h"
#include "drivers/rtc.h"

#include <cstdint>
#include <cstdlib>
#include <esp_log.h>
#include <sys/time.h>

static const char* TAG = "clock";

namespace {
Rtc* rtc = nullptr;
bool valid = false;
constexpr time_t Plausible = 1767225600;  // 2026-01-01
}  // namespace

namespace clock_svc {

void init(Rtc& r, const char* tz) {
	rtc = &r;
	setTimezone(tz);
	time_t utc = 0;
	bool ok = false;
	if (rtc->read(utc, ok) && ok && utc >= Plausible) {
		const timeval tv = { .tv_sec = utc, .tv_usec = 0 };
		settimeofday(&tv, nullptr);
		valid = true;
		ESP_LOGI(TAG, "time from RTC: %lld", (long long) utc);
	} else {
		ESP_LOGW(TAG, "RTC time not valid, waiting for sync");
	}
}

void setTimezone(const char* tz) {
	setenv("TZ", tz, 1);
	tzset();
}

void setUtc(time_t utc) {
	const timeval tv = { .tv_sec = utc, .tv_usec = 0 };
	settimeofday(&tv, nullptr);
	valid = utc >= Plausible;
	if (rtc && !rtc->write(utc)) ESP_LOGE(TAG, "RTC write failed");
	ESP_LOGI(TAG, "time set: %lld", (long long) utc);
}

bool isValid() {
	return valid;
}

tm local() {
	const time_t now = time(nullptr);
	tm t;
	localtime_r(&now, &t);
	return t;
}

time_t fromUtcFields(const tm& t) {
	// Days from civil date (Howard Hinnant), valid for the proleptic Gregorian calendar.
	int y = t.tm_year + 1900 + t.tm_mon / 12;
	const int m = t.tm_mon % 12 + 1;
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const int yoe = y - era * 400;
	const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + t.tm_mday - 1;
	const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	const int64_t days = int64_t(era) * 146097 + doe - 719468;
	return time_t(days * 86400 + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec);
}

int utcOffset(time_t utc) {
	tm t;
	localtime_r(&utc, &t);
	return int(fromUtcFields(t) - utc);
}

}  // namespace clock_svc
