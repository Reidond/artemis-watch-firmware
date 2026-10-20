#pragma once

#include <cstddef>
#include <cstdint>
#include <esp_lcd_panel_io.h>

// ST7735S 128x128 TFT on SPI2 (HW revision 3: controller RAM 128x160).
// Init sequence, MADCTL and window offsets replicate LovyanGFX Panel_ST7735S
// as configured by the stock firmware (offset_rotation 3, BGR, no inversion).
class Display {
public:
	static constexpr int Width = 128;
	static constexpr int Height = 128;

	using DoneCallback = void (*)(void* ctx);

	void init(bool flipped);
	// Must be set before the first draw(); called from the SPI ISR.
	void setDoneCallback(DoneCallback cb, void* ctx) {
		doneCtx = ctx;
		doneCb = cb;
	}
	void setFlipped(bool flipped);
	void setSleep(bool sleep);

	// Starts an async DMA transfer of big-endian RGB565 pixels into the
	// inclusive window (x1,y1)-(x2,y2). onDone fires when the transfer completes.
	void draw(int x1, int y1, int x2, int y2, const void* pixels, size_t bytes);

	// Blocking fill used before the UI starts (clears controller RAM garbage).
	void fill(uint16_t rgb565);
	void fillRect(int x1, int y1, int x2, int y2, uint16_t rgb565);

private:
	void command(uint8_t cmd, const uint8_t* params = nullptr, size_t len = 0);
	void setWindow(int x1, int y1, int x2, int y2);

	esp_lcd_panel_io_handle_t io = nullptr;
	DoneCallback doneCb = nullptr;
	void* doneCtx = nullptr;
	uint16_t colStart = 0;
	uint16_t rowStart = 0;
};
