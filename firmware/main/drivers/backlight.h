#pragma once

#include <cstdint>

// TFT backlight on an active-low pin, driven by LEDC PWM.
class Backlight {
public:
	void init();
	// 0..100 perceived brightness (gamma-corrected, never fully dark unless off()).
	void set(uint8_t percent);
	void fadeTo(uint8_t percent, uint32_t durationMs);
	void off();
	bool isOn() const { return current > 0; }

private:
	static uint32_t duty(uint8_t percent);
	uint8_t current = 0;
};
