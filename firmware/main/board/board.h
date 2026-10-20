#pragma once

#include <cstdint>
#include <driver/i2c_master.h>

namespace board {

struct Identity {
	uint16_t pid = 0;
	uint8_t revision = 0;
};

// Reads product id / hardware revision burned into efuse BLOCK3 by CircuitMess.
Identity identity();

// True when this firmware's pin map matches the hardware (revision 2 or 3).
bool supported(const Identity& id);

// Puts every LED and the RGB LED into a known "off" state and the backlight off.
// Safe to call before anything else is initialized.
void initSafeState();

// Shared I2C bus for RTC and IMU.
i2c_master_bus_handle_t i2c();

// Cuts power via the power latch. Does not return when on battery.
// While USB is plugged in the watch stays powered, so this falls back to a reboot.
[[noreturn]] void powerOff();

}  // namespace board
