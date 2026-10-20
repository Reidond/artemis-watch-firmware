#include "text.h"
#include "fonts/fonts.h"
#include "fonts/icons.h"

#include <cstdint>
#include <lvgl.h>

namespace {

constexpr uint32_t Invalid = 0xFFFFFFFF;

struct Replacement {
	uint32_t cp;
	const char* out;
};

// Emoji and symbols the fonts lack, mapped to the closest icon or ASCII.
// Anything else in the emoji ranges is dropped (see emojiRange()).
constexpr Replacement Replacements[] = {
	// Punctuation and letterlike symbols
	{ 0x2010, "-" }, { 0x2011, "-" }, { 0x2012, "-" }, { 0x2015, "\xE2\x80\x94" }, { 0x2024, "." },
	{ 0x2032, "'" }, { 0x2033, "\"" }, { 0x2039, "<" }, { 0x203A, ">" }, { 0x203C, "!!" }, { 0x2049, "!?" },
	{ 0x2122, "TM" }, { 0x2139, "i" }, { 0x2212, "-" }, { 0x2248, "~" }, { 0x2260, "!=" }, { 0x2264, "<=" },
	{ 0x2265, ">=" },
	// Arrows and media
	{ 0x2190, LV_SYMBOL_LEFT }, { 0x2191, LV_SYMBOL_UP }, { 0x2192, LV_SYMBOL_RIGHT }, { 0x2193, LV_SYMBOL_DOWN },
	{ 0x27A1, LV_SYMBOL_RIGHT }, { 0x2B05, LV_SYMBOL_LEFT }, { 0x2B06, LV_SYMBOL_UP }, { 0x2B07, LV_SYMBOL_DOWN },
	{ 0x25B6, LV_SYMBOL_PLAY }, { 0x25C0, LV_SYMBOL_LEFT }, { 0x23F8, LV_SYMBOL_PAUSE }, { 0x23F9, LV_SYMBOL_STOP },
	{ 0x23ED, LV_SYMBOL_NEXT }, { 0x23EE, LV_SYMBOL_PREV }, { 0x1F500, LV_SYMBOL_SHUFFLE }, { 0x1F501, LV_SYMBOL_LOOP },
	{ 0x1F504, LV_SYMBOL_REFRESH },
	// Hearts
	{ 0x2665, ICON_HEART }, { 0x2763, ICON_HEART }, { 0x2764, ICON_HEART }, { 0x1F493, ICON_HEART },
	{ 0x1F495, ICON_HEART }, { 0x1F496, ICON_HEART }, { 0x1F497, ICON_HEART }, { 0x1F498, ICON_HEART },
	{ 0x1F499, ICON_HEART }, { 0x1F49A, ICON_HEART }, { 0x1F49B, ICON_HEART }, { 0x1F49C, ICON_HEART },
	{ 0x1F49D, ICON_HEART }, { 0x1F49E, ICON_HEART }, { 0x1F49F, ICON_HEART }, { 0x1F5A4, ICON_HEART },
	{ 0x1F90D, ICON_HEART }, { 0x1F90E, ICON_HEART }, { 0x1F9E1, ICON_HEART }, { 0x1FA75, ICON_HEART },
	{ 0x1FA76, ICON_HEART }, { 0x1FA77, ICON_HEART }, { 0x1FAF6, ICON_HEART }, { 0x1F494, ICON_HEART_BROKEN },
	{ 0x1F48B, ICON_KISS },
	// Faces
	{ 0x263A, ICON_SMILE }, { 0x1F642, ICON_SMILE }, { 0x1F60A, ICON_SMILE }, { 0x1F607, ICON_SMILE },
	{ 0x1F643, ICON_SMILE }, { 0x1F60C, ICON_SMILE }, { 0x1F917, ICON_SMILE }, { 0x1F60E, ICON_SMILE },
	{ 0x1F600, ICON_LAUGH }, { 0x1F601, ICON_LAUGH }, { 0x1F603, ICON_LAUGH }, { 0x1F604, ICON_LAUGH },
	{ 0x1F605, ICON_LAUGH }, { 0x1F606, ICON_LAUGH }, { 0x1F602, ICON_GRIN_TEARS }, { 0x1F923, ICON_GRIN_TEARS },
	{ 0x1F929, ICON_GRIN_STARS }, { 0x1F973, ICON_GRIN_STARS }, { 0x1F60D, ICON_GRIN_HEARTS },
	{ 0x1F970, ICON_GRIN_HEARTS }, { 0x1F618, ICON_KISS }, { 0x1F617, ICON_KISS }, { 0x1F619, ICON_KISS },
	{ 0x1F61A, ICON_KISS }, { 0x1F60B, ICON_GRIN_TONGUE }, { 0x1F61B, ICON_GRIN_TONGUE },
	{ 0x1F61C, ICON_GRIN_TONGUE }, { 0x1F61D, ICON_GRIN_TONGUE }, { 0x1F92A, ICON_GRIN_TONGUE }, { 0x1F609, ICON_WINK },
	{ 0x1F610, ICON_MEH }, { 0x1F611, ICON_MEH }, { 0x1F612, ICON_MEH }, { 0x1F644, ICON_MEH }, { 0x1F60F, ICON_MEH },
	{ 0x1F636, ICON_MEH }, { 0x1F928, ICON_MEH }, { 0x1F62C, ICON_MEH }, { 0x1F914, ICON_MEH },
	{ 0x2639, ICON_FROWN }, { 0x1F641, ICON_FROWN }, { 0x1F61E, ICON_FROWN }, { 0x1F614, ICON_FROWN },
	{ 0x1F61F, ICON_FROWN }, { 0x1F615, ICON_FROWN }, { 0x1F623, ICON_FROWN }, { 0x1F616, ICON_FROWN },
	{ 0x1F629, ICON_FROWN }, { 0x1F62B, ICON_FROWN }, { 0x1F622, ICON_SAD_TEAR }, { 0x1F62D, ICON_SAD_TEAR },
	{ 0x1F97A, ICON_SAD_TEAR }, { 0x1F625, ICON_SAD_TEAR }, { 0x1F613, ICON_SAD_TEAR }, { 0x1F972, ICON_SAD_TEAR },
	{ 0x1F62E, ICON_SURPRISE }, { 0x1F62F, ICON_SURPRISE }, { 0x1F632, ICON_SURPRISE }, { 0x1F631, ICON_SURPRISE },
	{ 0x1F628, ICON_SURPRISE }, { 0x1F633, ICON_SURPRISE }, { 0x1F92F, ICON_SURPRISE }, { 0x1F626, ICON_SURPRISE },
	{ 0x1F627, ICON_SURPRISE }, { 0x1F630, ICON_SURPRISE }, { 0x1F620, ICON_ANGRY }, { 0x1F621, ICON_ANGRY },
	{ 0x1F92C, ICON_ANGRY }, { 0x1F624, ICON_ANGRY }, { 0x1F634, ICON_MOON }, { 0x1F4A4, ICON_MOON },
	// Hands
	{ 0x1F44D, ICON_THUMBS_UP }, { 0x1F44F, ICON_THUMBS_UP }, { 0x1F4AA, ICON_THUMBS_UP }, { 0x1F44C, ICON_THUMBS_UP },
	{ 0x270C, ICON_THUMBS_UP }, { 0x1F91E, ICON_THUMBS_UP }, { 0x1F44E, ICON_THUMBS_DOWN }, { 0x1F44B, ICON_HAND },
	{ 0x270B, ICON_HAND }, { 0x1F590, ICON_HAND }, { 0x1F91A, ICON_HAND }, { 0x1F64F, ICON_PRAYING_HANDS },
	{ 0x1F91D, ICON_HANDSHAKE },
	// Marks and alerts
	{ 0x1F525, ICON_FIRE }, { 0x2B50, ICON_STAR }, { 0x1F31F, ICON_STAR }, { 0x2728, ICON_STAR }, { 0x2605, ICON_STAR },
	{ 0x2606, ICON_STAR }, { 0x1F4AB, ICON_STAR }, { 0x2705, LV_SYMBOL_OK }, { 0x2714, LV_SYMBOL_OK },
	{ 0x2611, LV_SYMBOL_OK }, { 0x2713, LV_SYMBOL_OK }, { 0x274C, LV_SYMBOL_CLOSE }, { 0x2716, LV_SYMBOL_CLOSE },
	{ 0x274E, LV_SYMBOL_CLOSE }, { 0x2717, LV_SYMBOL_CLOSE }, { 0x2718, LV_SYMBOL_CLOSE }, { 0x26A0, LV_SYMBOL_WARNING },
	{ 0x1F6A8, LV_SYMBOL_WARNING }, { 0x26D4, LV_SYMBOL_WARNING }, { 0x1F6AB, LV_SYMBOL_WARNING },
	{ 0x26A1, LV_SYMBOL_CHARGE }, { 0x1F4A5, LV_SYMBOL_CHARGE }, { 0x2757, "!" }, { 0x2755, "!" }, { 0x2753, "?" },
	{ 0x2754, "?" }, { 0x2795, LV_SYMBOL_PLUS }, { 0x2796, LV_SYMBOL_MINUS }, { 0x1F4AF, "100" }, { 0x1F197, "OK" },
	{ 0x1F195, "NEW" }, { 0x1F198, "SOS" },
	{ 0x1F534, ICON_CIRCLE }, { 0x1F535, ICON_CIRCLE }, { 0x1F7E0, ICON_CIRCLE }, { 0x1F7E1, ICON_CIRCLE },
	{ 0x1F7E2, ICON_CIRCLE }, { 0x1F7E3, ICON_CIRCLE }, { 0x1F7E4, ICON_CIRCLE }, { 0x26AA, ICON_CIRCLE },
	{ 0x26AB, ICON_CIRCLE }, { 0x25CF, ICON_CIRCLE }, { 0x2B55, ICON_CIRCLE },
	// Objects
	{ 0x1F514, LV_SYMBOL_BELL }, { 0x1F4E2, ICON_BULLHORN }, { 0x1F4E3, ICON_BULLHORN }, { 0x1F3B5, LV_SYMBOL_AUDIO },
	{ 0x1F3B6, LV_SYMBOL_AUDIO }, { 0x1F3A7, LV_SYMBOL_AUDIO }, { 0x1F3A4, LV_SYMBOL_AUDIO }, { 0x1F4F7, ICON_CAMERA },
	{ 0x1F4F8, ICON_CAMERA }, { 0x1F3A5, LV_SYMBOL_VIDEO }, { 0x1F3AC, LV_SYMBOL_VIDEO }, { 0x1F4F9, LV_SYMBOL_VIDEO },
	{ 0x1F4FA, LV_SYMBOL_VIDEO }, { 0x1F4DE, LV_SYMBOL_CALL }, { 0x260E, LV_SYMBOL_CALL }, { 0x1F4F1, ICON_MOBILE },
	{ 0x1F4F2, ICON_MOBILE }, { 0x2709, LV_SYMBOL_ENVELOPE }, { 0x1F4E7, LV_SYMBOL_ENVELOPE },
	{ 0x1F4E8, LV_SYMBOL_ENVELOPE }, { 0x1F4E9, LV_SYMBOL_ENVELOPE }, { 0x1F48C, LV_SYMBOL_ENVELOPE },
	{ 0x1F4BB, ICON_LAPTOP }, { 0x1F3E0, LV_SYMBOL_HOME }, { 0x1F3E1, LV_SYMBOL_HOME }, { 0x1F4CD, ICON_MAP_MARKER },
	{ 0x1F4CC, ICON_MAP_MARKER }, { 0x1F30D, ICON_GLOBE }, { 0x1F30E, ICON_GLOBE }, { 0x1F30F, ICON_GLOBE },
	{ 0x1F310, ICON_GLOBE }, { 0x23F0, ICON_CLOCK }, { 0x23F1, ICON_CLOCK }, { 0x23F2, ICON_CLOCK }, { 0x231A, ICON_CLOCK },
	{ 0x231B, ICON_HOURGLASS }, { 0x23F3, ICON_HOURGLASS }, { 0x1F4C5, ICON_CALENDAR }, { 0x1F4C6, ICON_CALENDAR },
	{ 0x1F5D3, ICON_CALENDAR }, { 0x1F381, ICON_GIFT }, { 0x1F382, ICON_CAKE }, { 0x1F389, ICON_CHEERS },
	{ 0x1F38A, ICON_CHEERS }, { 0x1F942, ICON_CHEERS }, { 0x1F37B, ICON_CHEERS }, { 0x1F37E, ICON_CHEERS },
	{ 0x1F3C6, ICON_TROPHY }, { 0x1F947, ICON_TROPHY }, { 0x1F680, ICON_ROCKET }, { 0x1F6F0, ICON_ROCKET },
	{ 0x2708, ICON_PLANE }, { 0x1F6EB, ICON_PLANE }, { 0x1F6EC, ICON_PLANE }, { 0x1F697, ICON_CAR }, { 0x1F695, ICON_CAR },
	{ 0x1F699, ICON_CAR }, { 0x2615, ICON_COFFEE }, { 0x1F511, ICON_KEY }, { 0x1F5DD, ICON_KEY }, { 0x1F512, ICON_LOCK },
	{ 0x1F510, ICON_LOCK }, { 0x1F513, ICON_LOCK }, { 0x1F464, ICON_USER }, { 0x1F465, ICON_USERS }, { 0x1F46A, ICON_USERS },
	{ 0x1F4AC, ICON_COMMENT }, { 0x1F5E8, ICON_COMMENT }, { 0x1F4AD, ICON_COMMENT }, { 0x1F4F0, ICON_NEWSPAPER },
	{ 0x1F4BC, ICON_BRIEFCASE }, { 0x1F440, LV_SYMBOL_EYE_OPEN }, { 0x1F441, LV_SYMBOL_EYE_OPEN },
	{ 0x1F50B, LV_SYMBOL_BATTERY_FULL }, { 0x270F, LV_SYMBOL_EDIT }, { 0x1F4DD, LV_SYMBOL_EDIT },
	{ 0x2702, LV_SYMBOL_CUT }, { 0x1F5D1, LV_SYMBOL_TRASH }, { 0x2699, LV_SYMBOL_SETTINGS }, { 0x1F4F6, LV_SYMBOL_WIFI },
	// Weather
	{ 0x2600, ICON_SUN }, { 0x1F31E, ICON_SUN }, { 0x1F324, ICON_SUN }, { 0x1F319, ICON_MOON }, { 0x1F31B, ICON_MOON },
	{ 0x1F31C, ICON_MOON }, { 0x26C5, ICON_CLOUD }, { 0x2601, ICON_CLOUD }, { 0x1F325, ICON_CLOUD },
	{ 0x1F326, ICON_CLOUD }, { 0x1F327, ICON_CLOUD }, { 0x26C8, ICON_CLOUD }, { 0x1F329, ICON_CLOUD },
	{ 0x2614, ICON_CLOUD }, { 0x2602, ICON_CLOUD }, { 0x2744, ICON_SNOWFLAKE }, { 0x2603, ICON_SNOWFLAKE },
	{ 0x26C4, ICON_SNOWFLAKE }, { 0x1F328, ICON_SNOWFLAKE },
};

// U+0100..U+017F without diacritics (Polish, Czech, Baltic, Turkish, ...).
const char* const LatinExtA[128] = {
	"A", "a", "A", "a", "A", "a", "C", "c", "C", "c", "C", "c", "C", "c", "D", "d",
	"D", "d", "E", "e", "E", "e", "E", "e", "E", "e", "E", "e", "G", "g", "G", "g",
	"G", "g", "G", "g", "H", "h", "H", "h", "I", "i", "I", "i", "I", "i", "I", "i",
	"I", "i", "IJ", "ij", "J", "j", "K", "k", "k", "L", "l", "L", "l", "L", "l", "L",
	"l", "L", "l", "N", "n", "N", "n", "N", "n", "n", "N", "n", "O", "o", "O", "o",
	"O", "o", "OE", "oe", "R", "r", "R", "r", "R", "r", "S", "s", "S", "s", "S", "s",
	"S", "s", "T", "t", "T", "t", "T", "t", "U", "u", "U", "u", "U", "u", "U", "u",
	"U", "u", "U", "u", "W", "w", "Y", "y", "Y", "Z", "z", "Z", "z", "Z", "z", "s",
};

bool within(uint32_t cp, uint32_t lo, uint32_t hi) {
	return cp >= lo && cp <= hi;
}

// Decodes one code point at s[i] and advances i. Malformed bytes yield
// Invalid and advance by one, so decoding resyncs on the next lead byte.
uint32_t decode(std::string_view s, size_t& i) {
	const auto b = [&](size_t k) { return uint8_t(s[i + k]); };
	const uint8_t b0 = b(0);
	int len = b0 < 0x80 ? 1 : (b0 & 0xE0) == 0xC0 ? 2 : (b0 & 0xF0) == 0xE0 ? 3 : (b0 & 0xF8) == 0xF0 ? 4 : 0;
	if (len == 0 || i + len > s.size()) {
		i++;
		return Invalid;
	}
	uint32_t cp = len == 1 ? b0 : b0 & (0x7F >> len);
	for (int k = 1; k < len; k++) {
		if ((b(k) & 0xC0) != 0x80) {
			i++;
			return Invalid;
		}
		cp = (cp << 6) | (b(k) & 0x3F);
	}
	static constexpr uint32_t MinForLen[] = { 0, 0, 0x80, 0x800, 0x10000 };
	if (cp < MinForLen[len] || cp > 0x10FFFF || within(cp, 0xD800, 0xDFFF)) {
		i++;
		return Invalid;
	}
	i += len;
	return cp;
}

void encode(std::string& out, uint32_t cp) {
	if (cp < 0x80) {
		out += char(cp);
	} else if (cp < 0x800) {
		out += char(0xC0 | (cp >> 6));
		out += char(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		out += char(0xE0 | (cp >> 12));
		out += char(0x80 | ((cp >> 6) & 0x3F));
		out += char(0x80 | (cp & 0x3F));
	} else {
		out += char(0xF0 | (cp >> 18));
		out += char(0x80 | ((cp >> 12) & 0x3F));
		out += char(0x80 | ((cp >> 6) & 0x3F));
		out += char(0x80 | (cp & 0x3F));
	}
}

bool spaceLike(uint32_t cp) {
	return cp == ' ' || cp == '\t' || cp == 0xA0 || cp == 0x1680 || within(cp, 0x2000, 0x200A) || cp == 0x202F ||
		cp == 0x205F || cp == 0x3000 || cp == 0x2800;
}

// Formatting characters with no visible glyph. iOS wraps names in bidi
// isolates (U+2068..U+2069), emoji carry variation selectors and skin tones.
bool invisible(uint32_t cp) {
	return cp == 0xAD || cp == 0x34F || cp == 0x61C || cp == 0x180E || within(cp, 0x200B, 0x200F) ||
		within(cp, 0x202A, 0x202E) || within(cp, 0x2060, 0x206F) || cp == 0x20E3 || within(cp, 0xFE00, 0xFE0F) || cp == 0xFEFF ||
		within(cp, 0x1F3FB, 0x1F3FF) || within(cp, 0xE0000, 0xE01EF);
}

// Pictographic blocks: anything here the fonts lack is dropped, not boxed.
bool emojiRange(uint32_t cp) {
	return within(cp, 0x2190, 0x21FF) || within(cp, 0x2300, 0x23FF) || within(cp, 0x2460, 0x24FF) || within(cp, 0x25A0, 0x27BF) ||
		within(cp, 0x2900, 0x297F) || within(cp, 0x2B00, 0x2BFF) || cp == 0x3030 || cp == 0x303D || cp == 0x3297 ||
		cp == 0x3299 || within(cp, 0x1F000, 0x1FAFF);
}

const char* replacement(uint32_t cp) {
	if (within(cp, 0x1F550, 0x1F567)) return ICON_CLOCK;  // clock faces
	for (const auto& r : Replacements) {
		if (r.cp == cp) return r.out;
	}
	return nullptr;
}

bool hasGlyph(uint32_t cp) {
	lv_font_glyph_dsc_t dsc;
	return lv_font_get_glyph_dsc(&font_body, &dsc, cp, 0) && !dsc.is_placeholder;
}

}  // namespace

namespace text {

std::string display(std::string_view in, bool singleLine) {
	std::string out;
	out.reserve(in.size());
	int breaks = 0;          // line breaks seen since the last visible character
	bool space = false;      // whitespace seen since the last visible character
	bool afterZwj = false;   // the next pictograph is part of a ZWJ sequence
	bool afterCr = false;

	const auto separate = [&] {
		if (!out.empty()) {
			if (breaks && !singleLine) out.append(breaks > 1 ? 2 : 1, '\n');
			else if (breaks || space) out += ' ';
		}
		breaks = 0;
		space = false;
	};

	size_t i = 0;
	while (i < in.size()) {
		const uint32_t cp = decode(in, i);
		if (cp == Invalid) continue;
		const bool cr = cp == '\r';
		if (cr || cp == '\n' || cp == 0x2028 || cp == 0x2029 || cp == 0x85) {
			if (!(cp == '\n' && afterCr)) breaks++;
			afterCr = cr;
			continue;
		}
		afterCr = false;
		if (spaceLike(cp)) {
			space = true;
			continue;
		}
		if (cp == 0x200D) {
			afterZwj = true;
			continue;
		}
		if (cp < 0x20 || within(cp, 0x7F, 0x9F) || invisible(cp)) continue;
		// A ZWJ sequence (family, "heart on fire") collapses onto its first pictograph.
		if (afterZwj) {
			afterZwj = false;
			if (cp >= 0x2000) continue;
		}
		if (within(cp, 0xE000, 0xF8FF)) continue;  // private use: our icons live there
		if (cp < 0x7F) {
			separate();
			out += char(cp);
		} else if (const char* r = replacement(cp)) {
			separate();
			out += r;
		} else if (within(cp, 0x100, 0x17F)) {
			separate();
			out += LatinExtA[cp - 0x100];
		} else if (hasGlyph(cp) || !emojiRange(cp)) {
			// Unknown scripts still show placeholder boxes, so names keep their length.
			separate();
			encode(out, cp);
		}
	}
	return out;
}

}  // namespace text
