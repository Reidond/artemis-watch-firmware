#pragma once

#include "ui.h"

#include <functional>
#include <utility>
#include <string>
#include <vector>

struct ListItem {
	const char* icon = nullptr;           // LV_SYMBOL_* or nullptr
	std::function<std::string()> label;
	std::function<std::string()> value;   // optional second line, refreshed after every action
	std::function<void()> action;         // Select
};

// Title + vertically scrolling list (design/concepts/04-menu.png): the
// selected row gets a checker frame, a red bar and a red ▶. Up/Down move,
// Select activates, Back pops.
class ListScreen : public Screen {
public:
	void build(lv_obj_t* root) override;
	void onButton(const ButtonEvent& e) override;
	hints::Set controls() const override;

protected:
	virtual std::string title() = 0;
	virtual std::vector<ListItem> items() = 0;
	void refreshValues();
	// Rebuilds the rows from items(), e.g. when the data behind them changed.
	void reload();
	int selectedIndex() const { return selected; }
	void setSelectedIndex(int index) { selected = index; }
	lv_obj_t* contentArea() const { return content; }
	// Extra decoration on the screen root after the list is built (e.g. a logo).
	virtual void decorate(lv_obj_t* root) {}

private:
	void select(int index, bool animate);

	lv_obj_t* root = nullptr;
	lv_obj_t* content = nullptr;
	std::vector<ListItem> list;
	std::vector<lv_obj_t*> rows;
	std::vector<lv_obj_t*> values;
	std::vector<lv_obj_t*> texts;
	std::vector<int> textWidths;
	std::vector<lv_obj_t*> icons;
	std::vector<std::pair<lv_obj_t*, lv_obj_t*>> marks;  // ▶ and red bar of each row
	int selected = 0;
};
