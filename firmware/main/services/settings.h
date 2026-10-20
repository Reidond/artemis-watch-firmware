#pragma once

#include <cstdint>

enum class Lang : uint8_t { Uk = 0, En = 1 };

struct SettingsData {
	Lang language = Lang::Uk;
	uint8_t brightness = 60;     // 0..100
	uint8_t screenTimeoutIdx = 2; // index into ScreenTimeouts
	bool use24h = true;
	bool flipped = false;
	bool sounds = true;
	char timezone[48] = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // Europe/Kyiv, POSIX TZ
};

// Persists SettingsData as one NVS blob. Not thread-safe: touch it from the UI task only.
class Settings {
public:
	static constexpr uint16_t ScreenTimeouts[] = { 5, 10, 15, 30, 60, 120 };  // seconds
	static constexpr int ScreenTimeoutCount = sizeof(ScreenTimeouts) / sizeof(ScreenTimeouts[0]);

	void load();
	void save();
	// Called after every save (the companion app gets notified).
	void (*onSave)() = nullptr;
	SettingsData& get() { return data; }
	uint32_t screenTimeoutMs() const { return ScreenTimeouts[data.screenTimeoutIdx] * 1000u; }

private:
	SettingsData data;
};
