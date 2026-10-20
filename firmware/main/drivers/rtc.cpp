#include "rtc.h"

#include <esp_log.h>

static const char* TAG = "rtc";

namespace {

constexpr uint8_t Addr = 0x51;
constexpr int TimeoutMs = 50;

uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t dec2bcd(uint8_t v) { return uint8_t(((v / 10) << 4) | (v % 10)); }

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = unsigned(y - era * 400);
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return int64_t(era) * 146097 + int64_t(doe) - 719468;
}

}  // namespace

bool Rtc::init(i2c_master_bus_handle_t bus) {
	const i2c_device_config_t cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = Addr,
		.scl_speed_hz = 400000,
		.scl_wait_us = 0,
		.flags = { .disable_ack_check = 0 },
	};
	if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) return false;
	// Control/status 1 and 2: normal mode, no alarms/timers.
	const uint8_t clear[] = { 0x00, 0x00, 0x00 };
	const bool ok = i2c_master_transmit(dev, clear, sizeof(clear), TimeoutMs) == ESP_OK;
	if (!ok) ESP_LOGE(TAG, "BM8563 not responding");
	return ok;
}

bool Rtc::read(time_t& utc, bool& valid) {
	const uint8_t reg = 0x02;
	uint8_t d[7];
	if (i2c_master_transmit_receive(dev, &reg, 1, d, sizeof(d), TimeoutMs) != ESP_OK) return false;
	valid = !(d[0] & 0x80);
	const int sec = bcd2dec(d[0] & 0x7F);
	const int min = bcd2dec(d[1] & 0x7F);
	const int hour = bcd2dec(d[2] & 0x3F);
	const int day = bcd2dec(d[3] & 0x3F);
	const int month = bcd2dec(d[5] & 0x1F);
	const int year = 1900 + bcd2dec(d[6]) + ((d[5] & 0x80) ? 100 : 0);
	if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 59) {
		valid = false;
		utc = 0;
		return true;
	}
	utc = time_t(daysFromCivil(year, month, day) * 86400 + hour * 3600 + min * 60 + sec);
	return true;
}

bool Rtc::write(time_t utc) {
	tm t;
	gmtime_r(&utc, &t);
	uint8_t d[8];
	d[0] = 0x02;
	d[1] = dec2bcd(t.tm_sec);  // also clears VL
	d[2] = dec2bcd(t.tm_min);
	d[3] = dec2bcd(t.tm_hour);
	d[4] = dec2bcd(t.tm_mday);
	d[5] = dec2bcd(t.tm_wday);
	d[6] = dec2bcd(t.tm_mon + 1) | (t.tm_year >= 100 ? 0x80 : 0);
	d[7] = dec2bcd(t.tm_year % 100);
	return i2c_master_transmit(dev, d, sizeof(d), TimeoutMs) == ESP_OK;
}
