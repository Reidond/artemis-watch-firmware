// Notifications from the iPhone (ANCS): list, detail (also used as the popup
// for new ones) and the actions menu.
#include "screens.h"
#include "ui/fonts/icons.h"
#include "ui/list_screen.h"
#include "ui/theme.h"

#include "i18n/i18n.h"
#include "services/clock.h"

#include <cstdio>

using phone::Category;
using phone::Notification;

namespace {

bool listed(const Notification& n) {
	return n.category != Category::IncomingCall;
}

std::string preview(const Notification& n) {
	std::string s = n.message.empty() ? n.subtitle : n.message;
	for (auto& c : s) {
		if (c == '\n') c = ' ';
	}
	return s;
}

std::string stamp(const Notification& n) {
	if (!n.year) return "";
	char buf[24];
	const tm now = clock_svc::local();
	if (clock_svc::isValid() && now.tm_year + 1900 == n.year && now.tm_mon + 1 == n.month && now.tm_mday == n.day) {
		snprintf(buf, sizeof(buf), "%02u:%02u", n.hour, n.minute);
	} else {
		snprintf(buf, sizeof(buf), "%02u.%02u %02u:%02u", n.day, n.month, n.hour, n.minute);
	}
	return buf;
}

bool hasActions(const Notification& n) {
	return n.flags & (phone::flag::PositiveAction | phone::flag::NegativeAction);
}

class NotificationList : public ListScreen {
public:
	void build(lv_obj_t* root) override {
		seen = phone::revision();
		ListScreen::build(root);
		if (!uids.empty()) return;

		// Floating: the list's flex layout would stack these otherwise.
		lv_obj_t* box = lv_obj_create(contentArea());
		lv_obj_remove_style_all(box);
		lv_obj_add_flag(box, LV_OBJ_FLAG_FLOATING);
		lv_obj_set_size(box, 128, 128 - 20 - hints::Height);
		const char* hint = nullptr;
		if (!phone::bonded()) hint = tr(Str::PairHint);
		else if (phone::link() != phone::Link::Ready) hint = tr(Str::PhoneNotConnected);
		int y = 8;
		if (!hint) {
			lv_obj_t* icon = theme::label(box, &font_large, theme::muted(), LV_SYMBOL_BELL);
			lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 12);
			y = 40;
		}
		lv_obj_t* empty = theme::label(box, &font_body, theme::text(), tr(Str::NoNotifications));
		lv_obj_set_width(empty, 124);
		lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
		lv_label_set_long_mode(empty, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_align(empty, LV_ALIGN_TOP_MID, 0, y);
		if (hint) {
			lv_obj_t* h = theme::label(box, &font_small, theme::muted(), hint);
			lv_obj_set_width(h, 120);
			lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
			lv_label_set_long_mode(h, LV_LABEL_LONG_MODE_WRAP);
			lv_obj_align(h, LV_ALIGN_TOP_MID, 0, y + 26);
		}
	}

	uint32_t tickMs() const override { return 300; }

	void tick() override {
		if (phone::revision() == seen) return;
		// Keep the cursor on the same notification when new ones arrive above it.
		const int index = selectedIndex();
		const uint32_t uid = index < int(uids.size()) ? uids[index] : 0;
		reload();
		for (size_t i = 0; i < uids.size(); i++) {
			if (uids[i] == uid && int(i) != index) {
				setSelectedIndex(int(i));
				reload();
				break;
			}
		}
	}

protected:
	std::string title() override { return tr(Str::MenuNotifications); }

	std::vector<ListItem> items() override {
		std::vector<ListItem> out;
		uids.clear();
		for (const auto& n : phone::notifications()) {
			if (!listed(n)) continue;
			const std::string label = n.title.empty() ? phone::appName(n.appId) : n.title;
			std::string value = preview(n);
			if (value.empty()) value = phone::appName(n.appId);
			const uint32_t uid = n.uid;
			out.push_back({ categoryIcon(n.category), [label] { return label; }, [value] { return value; },
				[uid] { ui::push(makeNotificationDetail(uid, false)); } });
			uids.push_back(uid);
		}
		return out;
	}

private:
	uint32_t seen = 0;
	std::vector<uint32_t> uids;
};

class NotificationDetail;
NotificationDetail* openPopup = nullptr;

class NotificationDetail : public Screen {
public:
	NotificationDetail(uint32_t uid, bool popup) : uid(uid), popup(popup) {
		if (popup) openPopup = this;
	}
	~NotificationDetail() override {
		if (openPopup == this) openPopup = nullptr;
	}

	void build(lv_obj_t* root) override {
		this->root = root;
		const Notification* n = phone::find(uid);
		if (!n) {
			gone = true;
			return;
		}
		shown = *n;
		content = theme::banner(root, phone::appName(n->appId).c_str(), categoryIcon(n->category));
		lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
		lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
		lv_obj_set_scroll_dir(content, LV_DIR_VER);
		lv_obj_set_style_pad_left(content, 4, 0);
		lv_obj_set_style_pad_right(content, 4, 0);
		lv_obj_set_style_pad_top(content, 2, 0);
		lv_obj_set_style_pad_bottom(content, 4, 0);
		lv_obj_set_style_pad_row(content, 2, 0);

		// Sender in bold, then the text, then when it came.
		if (!n->title.empty()) wrapped(&font_title, theme::text(), n->title);
		if (!n->subtitle.empty()) wrapped(&font_small, theme::muted(), n->subtitle);
		if (!n->message.empty()) wrapped(&font_body, theme::text(), n->message);
		theme::label(content, &font_small, theme::muted(), stamp(*n).c_str());
	}

	uint32_t tickMs() const override { return 300; }

	void tick() override {
		const Notification* n = phone::find(uid);
		if (gone || !n) {
			ui::pop();
			return;
		}
		if (n->title != shown.title || n->subtitle != shown.subtitle || n->message != shown.message ||
			n->flags != shown.flags) {
			show(uid);
		}
	}

	bool transient() const override { return popup; }

	void onButton(const ButtonEvent& e) override {
		const bool step = e.action == ButtonAction::Press || e.action == ButtonAction::Repeat;
		if (e.button == Button::Up && step && content) lv_obj_scroll_by_bounded(content, 0, 32, LV_ANIM_ON);
		if (e.button == Button::Down && step && content) lv_obj_scroll_by_bounded(content, 0, -32, LV_ANIM_ON);
		if (e.action != ButtonAction::Click) return;
		if (e.button == Button::Select && hasActions(shown)) ui::push(makeActions());
		if (e.button == Button::Alt) ui::pop();
	}

	hints::Set controls() const override {
		// Up/Down only when there is more text than fits.
		const bool scrolls = content && (lv_obj_get_scroll_top(content) > 0 || lv_obj_get_scroll_bottom(content) > 0);
		const char* move = scrolls ? hints::Move : nullptr;
		return { move, move, !gone && hasActions(shown) ? HINT_MORE : nullptr, LV_SYMBOL_LEFT };
	}

	void show(uint32_t id) {
		uid = id;
		gone = false;
		content = nullptr;
		if (!root) return;
		lv_obj_clean(root);
		build(root);
	}

private:
	lv_obj_t* wrapped(const lv_font_t* font, lv_color_t color, const std::string& s) {
		lv_obj_t* l = theme::label(content, font, color, s.c_str());
		lv_obj_set_width(l, 120);
		lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
		return l;
	}

	std::unique_ptr<Screen> makeActions();

	uint32_t uid;
	bool popup;
	bool gone = false;
	Notification shown;
	lv_obj_t* root = nullptr;
	lv_obj_t* content = nullptr;
};

class Actions : public ListScreen {
public:
	explicit Actions(uint32_t uid) : uid(uid) {}

	void tick() override {
		if (!phone::find(uid)) ui::pop();
	}

protected:
	std::string title() override { return tr(Str::Actions); }

	std::vector<ListItem> items() override {
		std::vector<ListItem> out;
		const Notification* n = phone::find(uid);
		if (!n) return out;
		const uint32_t id = uid;
		// Back to where the notification was opened from: this menu, then the detail.
		const auto act = [id](bool positive) {
			phone::perform(id, positive);
			ui::pop();
			ui::pop();
		};
		if (n->flags & phone::flag::PositiveAction) {
			const std::string label = n->positive.empty() ? tr(Str::ActionPositive) : n->positive;
			out.push_back({ LV_SYMBOL_OK, [label] { return label; }, nullptr, [act] { act(true); } });
		}
		if (n->flags & phone::flag::NegativeAction) {
			const std::string label = n->negative.empty() ? tr(Str::ActionNegative) : n->negative;
			out.push_back({ LV_SYMBOL_CLOSE, [label] { return label; }, nullptr, [act] { act(false); } });
		}
		return out;
	}

private:
	uint32_t uid;
};

std::unique_ptr<Screen> NotificationDetail::makeActions() {
	return std::make_unique<Actions>(uid);
}

}  // namespace

const char* categoryIcon(Category c) {
	switch (c) {
		case Category::IncomingCall: return LV_SYMBOL_CALL;
		case Category::MissedCall: return ICON_PHONE_SLASH;
		case Category::Voicemail: return LV_SYMBOL_CALL;
		case Category::Social: return ICON_COMMENT;
		case Category::Schedule: return ICON_CALENDAR;
		case Category::Email: return LV_SYMBOL_ENVELOPE;
		case Category::News: return ICON_NEWSPAPER;
		case Category::Health: return ICON_HEARTBEAT;
		case Category::Business: return ICON_BRIEFCASE;
		case Category::Location: return ICON_MAP_MARKER;
		case Category::Entertainment: return LV_SYMBOL_VIDEO;
		default: return LV_SYMBOL_BELL;
	}
}

std::unique_ptr<Screen> makeNotifications() {
	return std::make_unique<NotificationList>();
}

std::unique_ptr<Screen> makeNotificationDetail(uint32_t uid, bool popup) {
	return std::make_unique<NotificationDetail>(uid, popup);
}

bool showInPopup(uint32_t uid) {
	if (!openPopup) return false;
	openPopup->show(uid);
	return true;
}
