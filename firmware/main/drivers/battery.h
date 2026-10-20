#pragma once

#include <atomic>
#include <cstdint>

enum class Charge : uint8_t { Unplugged, Charging, Full };

// Battery voltage through a 1:4 divider on ADC1 with a switchable 2.5 V
// reference for offset calibration. Port of the stock BatteryV3 logic.
class Battery {
public:
	static constexpr int EmptyMv = 3600;
	static constexpr int FullMv = 4150;

	void init();
	int millivolts() const { return mv.load(); }
	// Charge estimate from the last reading taken on battery power (USB inflates
	// the voltage). -1 when the watch has been on USB since boot.
	int percent() const;
	Charge charge() const { return chargeState.load(); }
	int calibrationOffsetMv() const { return calOffset; }

private:
	static void task(void* arg);
	float readPinMv();
	void calibrate();
	void sample(bool fresh);
	void readCharge();

	void* adc = nullptr;
	void* cali = nullptr;
	int channel = 0;
	float calOffset = 0;
	float ema = -1;
	std::atomic<int> mv{ 0 };
	std::atomic<int> unpluggedMv{ -1 };
	std::atomic<Charge> chargeState{ Charge::Unplugged };
};
