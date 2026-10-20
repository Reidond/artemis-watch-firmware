#include "screens.h"
#include "ui/art/sprites.h"
#include "ui/fonts/icons.h"
#include "ui/list_screen.h"

#include "i18n/i18n.h"

namespace {

class Menu : public ListScreen {
protected:
	std::string title() override { return tr(Str::AppName); }

	void decorate(lv_obj_t* root) override {
		lv_obj_t* logo = lv_image_create(root);
		lv_image_set_src(logo, &sprite_logo);
		lv_obj_align(logo, LV_ALIGN_TOP_RIGHT, -3, 0);
	}

	std::vector<ListItem> items() override {
		return {
			{ LV_SYMBOL_BELL, [] { return tr(Str::MenuNotifications); }, nullptr, [] { ui::push(makeNotifications()); } },
			{ LV_SYMBOL_AUDIO, [] { return tr(Str::MenuMusic); }, nullptr, [] { ui::push(makeMusic()); } },
			{ ICON_CLOUD, [] { return tr(Str::MenuWeather); }, nullptr, [] { ui::push(makeWeather()); } },
			{ LV_SYMBOL_PLAY, [] { return tr(Str::MenuStopwatch); }, nullptr, [] { ui::push(makeStopwatch()); } },
			{ LV_SYMBOL_SETTINGS, [] { return tr(Str::MenuSettings); }, nullptr, [] { ui::push(makeSettings()); } },
			{ LV_SYMBOL_LIST, [] { return tr(Str::MenuAbout); }, nullptr, [] { ui::push(makeAbout()); } },
		};
	}
};

}  // namespace

std::unique_ptr<Screen> makeMenu() {
	return std::make_unique<Menu>();
}
