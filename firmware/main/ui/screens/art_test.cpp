// Development screen for the dither engine: `tools/watch.sh cmd art`.
#include "screens.h"
#include "ui/art/moon.h"
#include "ui/dither.h"
#include "ui/theme.h"

#include <ctime>

namespace {

class ArtTest : public Screen {
public:
	void build(lv_obj_t* root) override {
		lv_obj_t* obj = canvas.create(root, 128, 116);
		lv_obj_set_pos(obj, 0, 0);
		canvas.clear(theme::bg());
		const float phases[] = { 0.12f, 0.25f, 0.5f, 0.75f };
		for (int i = 0; i < 4; i++) art::drawMoon(canvas, 16 + i * 32, 16, 14, phases[i], dither::grey());
		art::drawMoon(canvas, 22, 56, 20, art::moonPhase(time(nullptr)), dither::grey());
		// Tone ramps: grey and red.
		for (int x = 0; x < 60; x++) {
			for (int y = 0; y < 10; y++) {
				canvas.shade(66 + x, 46 + y, x / 59.0f, dither::grey());
				canvas.shade(66 + x, 60 + y, x / 59.0f, dither::red());
			}
		}
		canvas.commit();
		// Tiled pattern fill.
		lv_obj_t* box = lv_obj_create(root);
		lv_obj_remove_style_all(box);
		lv_obj_set_pos(box, 66, 76);
		lv_obj_set_size(box, 60, 36);
		dither::fill(box, 24, theme::muted(), theme::bg());
		theme::label(box, &font_body, theme::text(), "17:28");
	}

	void onButton(const ButtonEvent& e) override {
		if (e.button == Button::Alt && e.action == ButtonAction::Click) ui::pop();
	}

private:
	dither::Canvas canvas;
};

}  // namespace

std::unique_ptr<Screen> makeArtTest() {
	return std::make_unique<ArtTest>();
}
