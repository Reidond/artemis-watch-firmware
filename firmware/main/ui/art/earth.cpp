#include "earth.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float Deg = float(M_PI) / 180.0f;

bool land(float latDeg, float lonDeg) {
	lonDeg = std::fmod(lonDeg + 540.0f, 360.0f) - 180.0f;
	const int col = std::clamp(int((lonDeg + 180.0f) * art::EarthLandWidth / 360.0f), 0, art::EarthLandWidth - 1);
	const int row = std::clamp(int((90.0f - latDeg) * art::EarthLandHeight / 180.0f), 0, art::EarthLandHeight - 1);
	return art::EarthLand[row][col >> 3] & (0x80 >> (col & 7));
}

float smoothstep(float a, float b, float x) {
	const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3 - 2 * t);
}

// Subsolar point (declination, longitude) in radians. Good to about a degree,
// which is a fraction of a pixel on this globe.
void subsolar(time_t utc, float& lat, float& lon) {
	tm t;
	gmtime_r(&utc, &t);
	const float day = float(t.tm_yday);
	lat = -23.44f * Deg * std::cos(2.0f * float(M_PI) / 365.0f * (day + 10.0f));
	const float hours = t.tm_hour + t.tm_min / 60.0f + t.tm_sec / 3600.0f;
	lon = (12.0f - hours) * 15.0f * Deg;
}

}  // namespace

namespace art {

void drawEarth(dither::Canvas& canvas, int cx, int cy, int radius, time_t utc, float latDeg, float lonDeg,
	const dither::Ramp& ramp) {
	float sunLat, sunLon;
	subsolar(utc, sunLat, sunLon);
	const float lat0 = latDeg * Deg, lon0 = lonDeg * Deg;
	const float sinLat0 = std::sin(lat0), cosLat0 = std::cos(lat0);
	const float sinSun = std::sin(sunLat), cosSun = std::cos(sunLat);
	const float r = float(radius);

	for (int py = -radius; py <= radius; py++) {
		for (int px = -radius; px <= radius; px++) {
			const float x = (px + 0.5f) / r, y = -(py + 0.5f) / r;  // y up
			const float rho = std::sqrt(x * x + y * y);
			if (rho > 1.0f) continue;
			// Inverse orthographic projection.
			const float c = std::asin(std::min(rho, 1.0f));
			const float sinC = std::sin(c), cosC = std::cos(c);
			const float lat = rho < 1e-6f ? lat0 : std::asin(cosC * sinLat0 + y * sinC * cosLat0 / rho);
			const float lon = lon0 + std::atan2(x * sinC, rho * cosC * cosLat0 - y * sinC * sinLat0);
			// Cosine of the sun's zenith angle at this point.
			const float sun = std::sin(lat) * sinSun + std::cos(lat) * cosSun * std::cos(lon - sunLon);
			const float day = smoothstep(-0.10f, 0.10f, sun);
			const bool isLand = land(lat / Deg, lon / Deg);
			const float lit = isLand ? 0.95f : 0.40f;
			// Night land stays faintly visible so the globe reads after dark.
			const float dark = isLand ? 0.24f : 0.0f;
			canvas.shade(cx + px, cy + py, dark + (lit - dark) * day, ramp);
		}
	}
	// Outline, like the concept art: keeps the night side's edge visible.
	const lv_color_t edge = ramp.colors[ramp.count / 2];
	for (int i = 0; i < 8 * radius; i++) {
		const float a = i * 2.0f * float(M_PI) / (8 * radius);
		canvas.set(cx + int(std::lround((r + 0.5f) * std::cos(a))), cy + int(std::lround((r + 0.5f) * std::sin(a))), edge);
	}
}

}  // namespace art
