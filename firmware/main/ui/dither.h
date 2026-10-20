#pragma once

#include <cstdint>
#include <lvgl.h>

// Ordered (Bayer 8x8) dithering for the 1-bit-print look: every shade on
// screen is a pattern of a few flat palette colors, never a smooth blend.
namespace dither {

// Threshold for pixel (x, y), 0..63.
uint8_t threshold(int x, int y);

// A ramp of flat colors from dark to light (2..5 entries). A value 0..1 is
// spread across the ramp and the gap between two neighbours is dithered.
struct Ramp {
	const lv_color_t* colors;
	uint8_t count;
	lv_color_t at(int x, int y, float value) const;
};

// Ramps built from the theme palette.
const Ramp& grey();  // black, grey, off-white
const Ramp& red();   // black, red, off-white highlight

// 8x8 RGB565 tile of `tone` (0 = all bg, 64 = all fg), cached. For
// lv_obj_set_style_bg_image_src with bg_image_tiled.
const lv_image_dsc_t* tile(uint8_t tone, lv_color_t fg, lv_color_t bg);
// Dithered flat fill for an object's background.
void fill(lv_obj_t* obj, uint8_t tone, lv_color_t fg, lv_color_t bg);

// RGB565 pixel surface shown by an lv_canvas. Draw, then call commit().
class Canvas {
public:
	lv_obj_t* create(lv_obj_t* parent, int w, int h);
	int width() const { return w; }
	int height() const { return h; }
	void clear(lv_color_t color);
	void set(int x, int y, lv_color_t color);
	// Fill with a value 0..1 per pixel through a ramp.
	void shade(int x, int y, float value, const Ramp& ramp) { set(x, y, ramp.at(x + ox, y + oy, value)); }
	// Screen position of the canvas, so its pattern lines up with tiles elsewhere.
	void setOrigin(int x, int y) { ox = x; oy = y; }
	void commit();

private:
	lv_obj_t* obj = nullptr;
	lv_draw_buf_t* buf = nullptr;
	int w = 0, h = 0, ox = 0, oy = 0;
};

}  // namespace dither
