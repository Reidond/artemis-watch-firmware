#pragma once

#include "ui/dither.h"

#include <cstdint>
#include <ctime>

namespace art {

constexpr int EarthLandWidth = 256;
constexpr int EarthLandHeight = 128;
// Equirectangular land mask (tools/gen_art.py), 1 bit per pixel, MSB first.
extern const uint8_t EarthLand[EarthLandHeight][EarthLandWidth / 8];

// Globe seen from above (latDeg, lonDeg), with the real day/night line for
// `utc`: day land light, day sea grey, night side dark. Dithered through `ramp`.
void drawEarth(dither::Canvas& canvas, int cx, int cy, int radius, time_t utc, float latDeg, float lonDeg,
	const dither::Ramp& ramp);

}  // namespace art
