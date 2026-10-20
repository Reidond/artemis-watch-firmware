// Now playing on the iPhone (AMS), styled after design/concepts/06-music.png.
// Select: play/pause, Up/Down: previous/next track, hold Up/Down: volume.
#include "screens.h"
#include "ui/art/moon.h"
#include "ui/dither.h"
#include "ui/theme.h"

#include "i18n/i18n.h"

#include <cstdio>

using phone::MediaCommand;
using phone::MediaState;

namespace {

constexpr int BarX = 4, BarY = 36, BarW = 120, BarH = 7;
constexpr int VolumeBlocks = 10;

std::string minutes(float seconds) {
	const int s = seconds > 0 ? int(seconds) : 0;
	char buf[16];
	snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
	return buf;
}

lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color) {
	lv_obj_t* o = lv_obj_create(parent);
	lv_obj_remove_style_all(o);
	lv_obj_set_pos(o, x, y);
	lv_obj_set_size(o, w, h);
	lv_obj_set_style_bg_color(o, color, 0);
	lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
	return o;
}

class Music : public Screen {
public:
	void build(lv_obj_t* root) override {
		lv_obj_t* content = theme::titled(root, tr(Str::MenuMusic), LV_SYMBOL_AUDIO);
		seen = 0;

		empty = lv_obj_create(content);
		lv_obj_remove_style_all(empty);
		lv_obj_set_size(empty, 128, lv_obj_get_style_height(content, LV_PART_MAIN));
		emptyText = theme::label(empty, &font_body, theme::text());
		lv_obj_set_width(emptyText, 124);
		lv_obj_set_style_text_align(emptyText, LV_TEXT_ALIGN_CENTER, 0);
		lv_label_set_long_mode(emptyText, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_align(emptyText, LV_ALIGN_TOP_MID, 0, 36);
		emptyHint = theme::label(empty, &font_small, theme::muted());
		lv_obj_set_width(emptyHint, 120);
		lv_obj_set_style_text_align(emptyHint, LV_TEXT_ALIGN_CENTER, 0);
		lv_label_set_long_mode(emptyHint, LV_LABEL_LONG_MODE_WRAP);
		lv_obj_align(emptyHint, LV_ALIGN_TOP_MID, 0, 56);
		// A full Moon as the "record", like the concept.
		lv_obj_t* emptyDisc = emptyArt.create(empty, 30, 30);
		lv_obj_align(emptyDisc, LV_ALIGN_TOP_MID, 0, 2);
		drawDisc(emptyArt);

		player = lv_obj_create(content);
		lv_obj_remove_style_all(player);
		lv_obj_set_size(player, 128, lv_obj_get_style_height(content, LV_PART_MAIN));

		lv_obj_t* disc = discArt.create(player, 30, 30);
		lv_obj_set_pos(disc, 3, 2);
		drawDisc(discArt);

		title = theme::label(player, &font_body, theme::text());
		lv_obj_set_size(title, 89, lv_font_get_line_height(&font_body));
		lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
		lv_obj_set_pos(title, 37, 2);

		artist = theme::label(player, &font_small, theme::muted());
		lv_obj_set_size(artist, 89, lv_font_get_line_height(&font_small));
		lv_label_set_long_mode(artist, LV_LABEL_LONG_MODE_DOTS);
		lv_obj_set_pos(artist, 37, 18);

		// Progress: outlined track, checker fill, red playhead.
		lv_obj_t* track = lv_obj_create(player);
		lv_obj_remove_style_all(track);
		lv_obj_set_pos(track, BarX, BarY);
		lv_obj_set_size(track, BarW, BarH);
		lv_obj_set_style_border_width(track, 1, 0);
		lv_obj_set_style_border_color(track, theme::text(), 0);
		progress = lv_obj_create(player);
		lv_obj_remove_style_all(progress);
		lv_obj_set_pos(progress, BarX + 1, BarY + 1);
		lv_obj_set_size(progress, 0, BarH - 2);
		dither::fill(progress, 32, theme::text(), theme::bg());
		playhead = box(player, BarX, BarY - 1, 3, BarH + 2, theme::accent());

		elapsed = theme::label(player, &font_small, theme::text());
		lv_obj_set_pos(elapsed, BarX, BarY + BarH + 1);
		total = theme::label(player, &font_small, theme::muted());
		lv_obj_align(total, LV_ALIGN_TOP_RIGHT, -BarX, BarY + BarH + 1);

		lv_obj_t* prev = theme::label(player, &font_large, theme::text(), LV_SYMBOL_PREV);
		lv_obj_align(prev, LV_ALIGN_TOP_MID, -36, 59);
		play = theme::label(player, &font_large, theme::accent(), LV_SYMBOL_PLAY);
		lv_obj_align(play, LV_ALIGN_TOP_MID, 0, 59);
		lv_obj_t* next = theme::label(player, &font_large, theme::text(), LV_SYMBOL_NEXT);
		lv_obj_align(next, LV_ALIGN_TOP_MID, 36, 59);

		volumeRow = lv_obj_create(player);
		lv_obj_remove_style_all(volumeRow);
		lv_obj_set_pos(volumeRow, 4, 80);
		lv_obj_set_size(volumeRow, 120, 12);
		theme::label(volumeRow, &font_small, theme::text(), LV_SYMBOL_VOLUME_MID);
		for (int i = 0; i < VolumeBlocks; i++) blocks[i] = box(volumeRow, 20 + i * 7, 3, 5, 5, theme::muted());
		volumeText = theme::label(volumeRow, &font_small, theme::text());
		lv_obj_align(volumeText, LV_ALIGN_RIGHT_MID, 0, 0);
	}

	uint32_t tickMs() const override { return 500; }

	void tick() override {
		const auto& m = phone::media();
		const bool loaded = phone::hasMedia() && (!m.title.empty() || m.state != MediaState::Stopped);
		if (phone::revision() != seen) {
			seen = phone::revision();
			if (!phone::hasMedia()) {
				lv_label_set_text(emptyText, tr(phone::bonded() ? Str::PhoneNotConnected : Str::NotPaired));
				lv_label_set_text(emptyHint, "");
			} else {
				lv_label_set_text(emptyText, tr(Str::NothingPlaying));
				lv_label_set_text(emptyHint, tr(Str::PlayOnPhone));
			}
			setText(title, m.title.empty() ? m.player : m.title);
			setText(artist, m.artist.empty() ? m.album : m.artist);
			lv_label_set_text(total, m.duration > 0 ? minutes(m.duration).c_str() : "");
			const bool vol = m.volume >= 0;
			setHidden(volumeRow, !vol);
			if (vol) {
				const float v = m.volume > 1 ? 1 : m.volume;
				const int lit = int(v * VolumeBlocks + 0.5f);
				for (int i = 0; i < VolumeBlocks; i++) {
					lv_obj_set_style_bg_color(blocks[i], i < lit ? theme::text() : theme::muted(), 0);
				}
				char buf[8];
				snprintf(buf, sizeof(buf), "%d%%", int(v * 100 + 0.5f));
				lv_label_set_text(volumeText, buf);
			}
		}
		setHidden(empty, loaded);
		setHidden(player, !loaded);
		if (!loaded) return;
		lv_label_set_text(play, m.state == MediaState::Playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
		const float pos = m.position();
		lv_label_set_text(elapsed, minutes(pos).c_str());
		const int px = m.duration > 0 ? int((BarW - 2) * (pos > m.duration ? 1.0f : pos / m.duration)) : 0;
		lv_obj_set_width(progress, px);
		lv_obj_set_x(playhead, BarX + px);
	}

	void onButton(const ButtonEvent& e) override {
		const bool held = e.action == ButtonAction::Long || e.action == ButtonAction::Repeat;
		const bool click = e.action == ButtonAction::Click;
		switch (e.button) {
			case Button::Select:
				if (click) phone::command(MediaCommand::Toggle);
				break;
			case Button::Up:
				if (click) phone::command(MediaCommand::Previous);
				if (held) phone::command(MediaCommand::VolumeUp);
				break;
			case Button::Down:
				if (click) phone::command(MediaCommand::Next);
				if (held) phone::command(MediaCommand::VolumeDown);
				break;
			case Button::Alt:
				if (click) ui::pop();
				break;
		}
	}

	hints::Set controls() const override {
		if (!phone::hasMedia()) return { .back = LV_SYMBOL_LEFT };
		const bool playing = phone::media().state == MediaState::Playing;
		return { LV_SYMBOL_PREV, LV_SYMBOL_NEXT, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY, LV_SYMBOL_LEFT };
	}

private:
	static void drawDisc(dither::Canvas& c) {
		c.clear(theme::bg());
		art::drawMoon(c, 15, 15, 14, 0.5f, dither::grey());
		for (int y = 13; y <= 17; y++) {
			for (int x = 13; x <= 17; x++) c.set(x, y, (x == 15 && y == 15) ? theme::bg() : theme::accent());
		}
		c.commit();
	}

	static void setHidden(lv_obj_t* o, bool hidden) {
		if (hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
		else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
	}

	// Setting the same text restarts the scroll animation, so only on change.
	static void setText(lv_obj_t* label, const std::string& s) {
		if (s != lv_label_get_text(label)) lv_label_set_text(label, s.c_str());
	}

	uint32_t seen = 0;
	dither::Canvas discArt, emptyArt;
	lv_obj_t* empty = nullptr;
	lv_obj_t* emptyText = nullptr;
	lv_obj_t* emptyHint = nullptr;
	lv_obj_t* player = nullptr;
	lv_obj_t* title = nullptr;
	lv_obj_t* artist = nullptr;
	lv_obj_t* progress = nullptr;
	lv_obj_t* playhead = nullptr;
	lv_obj_t* elapsed = nullptr;
	lv_obj_t* total = nullptr;
	lv_obj_t* play = nullptr;
	lv_obj_t* volumeRow = nullptr;
	lv_obj_t* volumeText = nullptr;
	lv_obj_t* blocks[VolumeBlocks] = {};
};

}  // namespace

std::unique_ptr<Screen> makeMusic() {
	return std::make_unique<Music>();
}
