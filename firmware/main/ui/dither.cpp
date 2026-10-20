#include "dither.h"
#include "theme.h"

#include <algorithm>
#include <cmath>
#include <esp_heap_caps.h>
#include <map>
#include <tuple>

namespace {

constexpr uint8_t Bayer8[8][8] = {
	{ 0, 32, 8, 40, 2, 34, 10, 42 },
	{ 48, 16, 56, 24, 50, 18, 58, 26 },
	{ 12, 44, 4, 36, 14, 46, 6, 38 },
	{ 60, 28, 52, 20, 62, 30, 54, 22 },
	{ 3, 35, 11, 43, 1, 33, 9, 41 },
	{ 51, 19, 59, 27, 49, 17, 57, 25 },
	{ 15, 47, 7, 39, 13, 45, 5, 37 },
	{ 63, 31, 55, 23, 61, 29, 53, 21 },
};

struct TileKey {
	uint8_t tone;
	uint16_t fg, bg;
	bool operator<(const TileKey& o) const { return std::tie(tone, fg, bg) < std::tie(o.tone, o.fg, o.bg); }
};

std::map<TileKey, lv_image_dsc_t*> tiles;

}  // namespace

namespace dither {

uint8_t threshold(int x, int y) {
	return Bayer8[y & 7][x & 7];
}

lv_color_t Ramp::at(int x, int y, float value) const {
	value = std::clamp(value, 0.0f, 1.0f) * (count - 1);
	const int base = std::min(int(value), count - 1);
	if (base == count - 1) return colors[base];
	const int frac = int((value - base) * 64.0f);
	return colors[frac > threshold(x, y) ? base + 1 : base];
}

const Ramp& grey() {
	static const lv_color_t colors[] = { lv_color_hex(0x000000), lv_color_hex(0x808080), lv_color_hex(0xF2F2F2) };
	static const Ramp ramp{ colors, 3 };
	return ramp;
}

const Ramp& red() {
	static const lv_color_t colors[] = { lv_color_hex(0x000000), lv_color_hex(0xFC3D21), lv_color_hex(0xF2F2F2) };
	static const Ramp ramp{ colors, 3 };
	return ramp;
}

const lv_image_dsc_t* tile(uint8_t tone, lv_color_t fg, lv_color_t bg) {
	tone = std::min<uint8_t>(tone, 64);
	const TileKey key{ tone, lv_color_to_u16(fg), lv_color_to_u16(bg) };
	auto it = tiles.find(key);
	if (it != tiles.end()) return it->second;

	auto* px = static_cast<uint16_t*>(heap_caps_malloc(8 * 8 * 2, MALLOC_CAP_8BIT));
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) px[y * 8 + x] = tone > threshold(x, y) ? key.fg : key.bg;
	}
	auto* dsc = new lv_image_dsc_t{};
	dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
	dsc->header.cf = LV_COLOR_FORMAT_RGB565;
	dsc->header.w = 8;
	dsc->header.h = 8;
	dsc->header.stride = 16;
	dsc->data_size = 8 * 8 * 2;
	dsc->data = reinterpret_cast<const uint8_t*>(px);
	tiles[key] = dsc;
	return dsc;
}

void fill(lv_obj_t* obj, uint8_t tone, lv_color_t fg, lv_color_t bg) {
	lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
	lv_obj_set_style_bg_image_src(obj, tile(tone, fg, bg), 0);
	lv_obj_set_style_bg_image_tiled(obj, true, 0);
}

lv_obj_t* Canvas::create(lv_obj_t* parent, int width, int height) {
	w = width;
	h = height;
	buf = lv_draw_buf_create(w, h, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
	obj = lv_canvas_create(parent);
	lv_canvas_set_draw_buf(obj, buf);
	// The draw buffer outlives nothing but the canvas; free it with the object.
	lv_obj_add_event_cb(
		obj, [](lv_event_t* e) { lv_draw_buf_destroy(static_cast<lv_draw_buf_t*>(lv_event_get_user_data(e))); },
		LV_EVENT_DELETE, buf);
	clear(theme::bg());
	return obj;
}

void Canvas::clear(lv_color_t color) {
	const uint16_t c = lv_color_to_u16(color);
	for (int y = 0; y < h; y++) {
		auto* row = reinterpret_cast<uint16_t*>(buf->data + y * buf->header.stride);
		std::fill(row, row + w, c);
	}
}

void Canvas::set(int x, int y, lv_color_t color) {
	if (x < 0 || y < 0 || x >= w || y >= h) return;
	reinterpret_cast<uint16_t*>(buf->data + y * buf->header.stride)[x] = lv_color_to_u16(color);
}

void Canvas::commit() {
	lv_obj_invalidate(obj);
}

}  // namespace dither
