#include "display.h"
#include "board/pins.h"

#include <algorithm>
#include <cstring>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_io_spi.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "display";

namespace {

constexpr spi_host_device_t Host = SPI2_HOST;
constexpr int PixelClock = 40 * 1000 * 1000;
constexpr int MaxTransfer = Display::Width * 40 * 2;

// MADCTL bits
constexpr uint8_t MY = 0x80, MX = 0x40, MV = 0x20, ML = 0x10, BGR = 0x08, MH = 0x04;

// LovyanGFX internal rotation 3 (stock default) and 1 (stock "rotate screen").
constexpr uint8_t MadctlNormal = MV | MY | ML | BGR;   // 0xB8
constexpr uint8_t MadctlFlipped = MV | MX | MH | BGR;  // 0x6C

// Rotation 3 on 128x160 RAM: columns start at 160 - 128 = 32. Rotation 1: no offset.
constexpr uint16_t ColStartNormal = 32;
constexpr uint16_t ColStartFlipped = 0;

struct InitCmd {
	uint8_t cmd;
	uint8_t len;
	uint8_t data[16];
	uint16_t delayMs;
};

// Panel_ST7735S Rcmd1 + Rcmd2 (without DISPON, sent after RAM is cleared).
const InitCmd InitSequence[] = {
	{ 0x01, 0, {}, 150 },                                    // SWRESET
	{ 0x11, 0, {}, 500 },                                    // SLPOUT
	{ 0xB1, 3, { 0x01, 0x2C, 0x2D }, 0 },                    // FRMCTR1
	{ 0xB2, 3, { 0x01, 0x2C, 0x2D }, 0 },                    // FRMCTR2
	{ 0xB3, 6, { 0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D }, 0 },  // FRMCTR3
	{ 0xB4, 1, { 0x07 }, 0 },                                // INVCTR
	{ 0xC0, 3, { 0xA2, 0x02, 0x84 }, 0 },                    // PWCTR1
	{ 0xC1, 1, { 0xC5 }, 0 },                                // PWCTR2
	{ 0xC2, 2, { 0x0A, 0x00 }, 0 },                          // PWCTR3
	{ 0xC3, 2, { 0x8A, 0x2A }, 0 },                          // PWCTR4
	{ 0xC4, 2, { 0x8A, 0xEE }, 0 },                          // PWCTR5
	{ 0xC5, 1, { 0x0E }, 0 },                                // VMCTR1
	{ 0xE0, 16, { 0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10 }, 0 },
	{ 0xE1, 16, { 0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10 }, 0 },
	{ 0x13, 0, {}, 10 },                                     // NORON
	{ 0x3A, 1, { 0x55 }, 0 },                                // COLMOD: RGB565
	{ 0x20, 0, {}, 0 },                                      // INVOFF
};

}  // namespace

void Display::init(bool flipped) {
	const spi_bus_config_t bus = {
		.mosi_io_num = pins::TftMosi,
		.miso_io_num = -1,
		.sclk_io_num = pins::TftSck,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = MaxTransfer,
	};
	ESP_ERROR_CHECK(spi_bus_initialize(Host, &bus, SPI_DMA_CH_AUTO));

	esp_lcd_panel_io_spi_config_t ioCfg = {};
	ioCfg.cs_gpio_num = -1;
	ioCfg.dc_gpio_num = pins::TftDc;
	ioCfg.spi_mode = 0;
	ioCfg.pclk_hz = PixelClock;
	ioCfg.trans_queue_depth = 4;
	ioCfg.lcd_cmd_bits = 8;
	ioCfg.lcd_param_bits = 8;
	ioCfg.on_color_trans_done = [](esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* user) -> bool {
		auto* self = static_cast<Display*>(user);
		if (self->doneCb) self->doneCb(self->doneCtx);
		return false;
	};
	ioCfg.user_ctx = this;
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) Host, &ioCfg, &io));

	// Hardware reset
	gpio_set_direction(pins::TftRst, GPIO_MODE_OUTPUT);
	gpio_set_level(pins::TftRst, 1);
	vTaskDelay(pdMS_TO_TICKS(5));
	gpio_set_level(pins::TftRst, 0);
	vTaskDelay(pdMS_TO_TICKS(10));
	gpio_set_level(pins::TftRst, 1);
	vTaskDelay(pdMS_TO_TICKS(120));

	for (const auto& c : InitSequence) {
		command(c.cmd, c.len ? c.data : nullptr, c.len);
		if (c.delayMs) vTaskDelay(pdMS_TO_TICKS(c.delayMs));
	}
	setFlipped(flipped);
	fill(0x0000);
	command(0x29);  // DISPON
	vTaskDelay(pdMS_TO_TICKS(100));
	ESP_LOGI(TAG, "ST7735S ready (%s)", flipped ? "flipped" : "normal");
}

void Display::setFlipped(bool flipped) {
	const uint8_t madctl = flipped ? MadctlFlipped : MadctlNormal;
	colStart = flipped ? ColStartFlipped : ColStartNormal;
	rowStart = 0;
	command(0x36, &madctl, 1);
}

void Display::setSleep(bool sleep) {
	command(sleep ? 0x10 : 0x11);
	vTaskDelay(pdMS_TO_TICKS(sleep ? 5 : 120));
}

void Display::command(uint8_t cmd, const uint8_t* params, size_t len) {
	ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, cmd, params, len));
}

void Display::setWindow(int x1, int y1, int x2, int y2) {
	const uint16_t xs = x1 + colStart, xe = x2 + colStart;
	const uint16_t ys = y1 + rowStart, ye = y2 + rowStart;
	const uint8_t caset[] = { uint8_t(xs >> 8), uint8_t(xs), uint8_t(xe >> 8), uint8_t(xe) };
	const uint8_t raset[] = { uint8_t(ys >> 8), uint8_t(ys), uint8_t(ye >> 8), uint8_t(ye) };
	command(0x2A, caset, sizeof(caset));
	command(0x2B, raset, sizeof(raset));
}

void Display::draw(int x1, int y1, int x2, int y2, const void* pixels, size_t bytes) {
	setWindow(x1, y1, x2, y2);
	ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, 0x2C, pixels, bytes));
}

void Display::fill(uint16_t rgb565) {
	fillRect(0, 0, Width - 1, Height - 1, rgb565);
}

void Display::fillRect(int x1, int y1, int x2, int y2, uint16_t rgb565) {
	constexpr int MaxPixels = Width * 16;
	auto* buf = static_cast<uint16_t*>(heap_caps_malloc(MaxPixels * 2, MALLOC_CAP_DMA));
	const uint16_t be = (rgb565 >> 8) | (rgb565 << 8);
	for (int i = 0; i < MaxPixels; i++) buf[i] = be;
	const int w = x2 - x1 + 1;
	const int rowsPerChunk = MaxPixels / w;
	for (int y = y1; y <= y2; y += rowsPerChunk) {
		const int ye = std::min(y + rowsPerChunk - 1, y2);
		setWindow(x1, y, x2, ye);
		// tx_param is synchronous; use it so the buffer can be freed right after.
		ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, 0x2C, buf, size_t(w) * (ye - y + 1) * 2));
	}
	heap_caps_free(buf);
}
