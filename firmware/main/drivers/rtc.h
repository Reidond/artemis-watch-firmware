#pragma once

#include <ctime>
#include <driver/i2c_master.h>

// BM8563 (PCF8563-compatible) real-time clock at I2C 0x51.
// This firmware keeps the RTC in UTC; the time zone is applied in software.
class Rtc {
public:
	bool init(i2c_master_bus_handle_t bus);
	// Returns false on I2C failure. `valid` is false when the RTC lost power
	// (VL flag) and the time cannot be trusted.
	bool read(time_t& utc, bool& valid);
	bool write(time_t utc);

private:
	i2c_master_dev_handle_t dev = nullptr;
};
