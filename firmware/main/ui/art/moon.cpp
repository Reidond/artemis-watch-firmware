#include "moon.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double SynodicMonth = 29.530588853 * 86400.0;
constexpr time_t ReferenceNewMoon = 947182440;  // 2000-01-06 18:14 UTC

// Dark lunar seas on a unit disc (x right/east on the sky, y down), with
// their albedo relative to the bright highlands.
struct Mare {
	float x, y, rx, ry, albedo;
};
constexpr Mare Maria[] = {
	{ -0.58f, -0.02f, 0.30f, 0.52f, 0.69f },  // Oceanus Procellarum
	{ -0.28f, -0.42f, 0.27f, 0.24f, 0.66f },  // Mare Imbrium
	{ 0.13f, -0.38f, 0.16f, 0.15f, 0.69f },   // Mare Serenitatis
	{ 0.31f, -0.08f, 0.21f, 0.18f, 0.69f },   // Mare Tranquillitatis
	{ 0.68f, -0.30f, 0.11f, 0.10f, 0.66f },   // Mare Crisium
	{ 0.55f, 0.20f, 0.12f, 0.15f, 0.72f },    // Mare Fecunditatis
	{ 0.35f, 0.33f, 0.09f, 0.09f, 0.72f },    // Mare Nectaris
	{ -0.18f, 0.38f, 0.17f, 0.13f, 0.72f },   // Mare Nubium
	{ -0.55f, 0.40f, 0.10f, 0.10f, 0.70f },   // Mare Humorum
	{ 0.05f, -0.70f, 0.45f, 0.07f, 0.76f },   // Mare Frigoris
};
// Young bright craters.
constexpr Mare Craters[] = {
	{ -0.15f, 0.72f, 0.05f, 0.05f, 1.35f },  // Tycho
	{ -0.33f, -0.12f, 0.04f, 0.04f, 1.25f },  // Copernicus
	{ -0.62f, -0.30f, 0.03f, 0.03f, 1.30f },  // Aristarchus
};

float smoothstep(float a, float b, float x) {
	const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3 - 2 * t);
}

// Cheap repeatable noise for small-crater texture.
float hash(int x, int y) {
	uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return float(h & 0xFFFF) / 65535.0f;
}

float albedo(float x, float y, int px, int py) {
	float a = 1.0f;
	for (const auto& m : Maria) {
		const float dx = (x - m.x) / m.rx, dy = (y - m.y) / m.ry;
		const float d = dx * dx + dy * dy;
		a = std::min(a, 1.0f - (1.0f - m.albedo) * (1.0f - smoothstep(0.35f, 1.0f, d)));
	}
	for (const auto& c : Craters) {
		const float dx = (x - c.x) / c.rx, dy = (y - c.y) / c.ry;
		if (dx * dx + dy * dy < 1.0f) a = c.albedo;
	}
	return a * (0.9f + 0.2f * hash(px, py));
}

}  // namespace

namespace art {

float moonPhase(time_t utc) {
	const double age = std::fmod(double(utc - ReferenceNewMoon), SynodicMonth);
	return float((age < 0 ? age + SynodicMonth : age) / SynodicMonth);
}

void drawMoon(dither::Canvas& canvas, int cx, int cy, int radius, float phase, const dither::Ramp& ramp) {
	const float angle = phase * 2.0f * float(M_PI);
	// Sun direction: from behind the Moon at new, from the viewer at full,
	// from the right (east on the sky) while waxing.
	const float lx = std::sin(angle), lz = -std::cos(angle);
	const float r = float(radius);
	for (int py = -radius; py <= radius; py++) {
		for (int px = -radius; px <= radius; px++) {
			const float x = (px + 0.5f) / r, y = (py + 0.5f) / r;
			const float rr = x * x + y * y;
			if (rr > 1.0f) continue;
			const float z = std::sqrt(1.0f - rr);
			const float lit = smoothstep(-0.04f, 0.12f, x * lx + z * lz);
			const float a = albedo(x, y, px, py);
			// Lit side nearly flat (the Moon barely darkens toward its limb),
			// faint earthshine on the dark side.
			const float value = lit * a * 0.92f + (1.0f - lit) * 0.05f * a;
			canvas.shade(cx + px, cy + py, value, ramp);
		}
	}
}

}  // namespace art
