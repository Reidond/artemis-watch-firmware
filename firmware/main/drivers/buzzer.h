#pragma once

#include <cstdint>
#include <initializer_list>

struct Tone {
	uint16_t freq;  // Hz, 0 = silence
	uint16_t ms;
};

// Piezo buzzer on LEDC. Melodies play on a background task.
class Buzzer {
public:
	void init();
	void play(std::initializer_list<Tone> tones, bool evenIfMuted = false);
	void setEnabled(bool enabled) { this->enabled = enabled; }

	void click() { play({ { 2200, 12 } }); }
	void chime() { play({ { 1320, 70 }, { 0, 30 }, { 1760, 110 } }); }
	void ring() { play({ { 1568, 90 }, { 0, 50 }, { 1568, 90 }, { 0, 50 }, { 2093, 160 } }); }

private:
	static void task(void* arg);
	void tone(uint16_t freq);

	static constexpr int MaxTones = 16;
	struct Melody {
		Tone tones[MaxTones];
		uint8_t count;
	};
	void* queue = nullptr;
	bool enabled = true;
};
