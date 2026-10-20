#!/usr/bin/env bash
# Regenerates the LVGL fonts. Text fonts cover Latin, Latin-1, full basic
# Cyrillic (incl. Ukrainian і ї є ґ ʼ), typographic punctuation, № and ₴, plus
# LVGL's built-in FontAwesome symbols and the extra icons in
# firmware/main/ui/fonts/icons.h (emoji stand-ins, categories).
#
#   tools/gen_fonts.sh              # Iosevka (SIL OFL) -> firmware/main/ui/fonts/, checked in
#   tools/gen_fonts.sh pragmata     # PragmataPro -> firmware/main/ui/fonts/local/, git-ignored
#
# Iosevka is open and has PragmataPro's narrow 0.5 em width, so both sets fit
# the same layouts. When fonts/local/ exists the build uses it instead of the
# matching checked-in files (firmware/main/CMakeLists.txt); run
# `tools/idf.sh reconfigure` after creating or deleting it. PragmataPro is a
# commercial font: its generated files must stay out of git.
#
# PRAGMATA_DIR / PRAGMATA_VER locate PragmataPro (default ~/Library/Fonts, 0903).
set -euo pipefail

# Relative paths keep the "Opts:" comment in generated files machine-independent.
cd "$(dirname "$0")/.."
FAMILY="${1:-iosevka}"
SRC="tools/.fonts-src"
LVGL_FONTS="firmware/managed_components/lvgl__lvgl/scripts/built_in_font"
CONV=(npx --yes lv_font_conv@1.5.3)
# 1 bit per pixel: hard pixel edges for the dithered look. BPP=4 brings back antialiasing.
BPP="${BPP:-1}"

TEXT="0x20-0x7E,0xA0-0xFF,0x2BC,0x400-0x45F,0x490-0x491,0x2013-0x2014,0x2018-0x201E,0x2022,0x2026,0x2116,0x20AC"
SYMBOLS="61441,61448,61451,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650"
# Keep in sync with firmware/main/ui/fonts/icons.h.
ICONS="61444,61445,61447,61463,61475,61488,61547,61549,61554,61557,61572,61585,61601,61612,61617,61632,61634,61684,61705,61713,61720,61721,61722,61747,61749,61796,61797,61829,61830,61881,61930,61949,61982,62036,62038,62133,62172,62405,62413,62429,62682,62806,62852,62855,62856,62857,62872,62874,62900,62914,63108,63391,63401"
AWESOME="$LVGL_FONTS/FontAwesome5-Solid+Brands+Regular.woff"

if [[ ! -f "$AWESOME" ]]; then
	echo "Run tools/idf.sh reconfigure first (downloads the LVGL component)." >&2
	exit 1
fi
mkdir -p "$SRC"

case "$FAMILY" in
	iosevka)
		OUT="firmware/main/ui/fonts"
		for w in 400 700; do
			[[ -f "$SRC/Iosevka-$w.woff" ]] ||
				curl -fsSL -o "$SRC/Iosevka-$w.woff" \
					"https://cdn.jsdelivr.net/npm/@fontsource/iosevka@5.3.0/files/iosevka-latin-$w-normal.woff"
		done
		REGULAR="$SRC/Iosevka-400.woff"
		BOLD="$SRC/Iosevka-700.woff"
		HRYVNIA=()  # Iosevka has ₴
		TEXT="$TEXT,0x20B4"
		;;
	pragmata)
		OUT="firmware/main/ui/fonts/local"
		PRAGMATA_DIR="${PRAGMATA_DIR:-$HOME/Library/Fonts}"
		PRAGMATA_VER="${PRAGMATA_VER:-0903}"
		for style in R B; do
			font="$PRAGMATA_DIR/PragmataPro${style}_${PRAGMATA_VER}.ttf"
			if [[ ! -f "$font" ]]; then
				echo "Missing $font (set PRAGMATA_DIR / PRAGMATA_VER)." >&2
				exit 1
			fi
			ln -sf "$font" "$SRC/PragmataPro-$style.ttf"
		done
		REGULAR="$SRC/PragmataPro-R.ttf"
		BOLD="$SRC/PragmataPro-B.ttf"
		HRYVNIA=(--font "$LVGL_FONTS/Montserrat-Medium.ttf" -r 0x20B4)  # PragmataPro has no ₴
		;;
	*)
		echo "usage: tools/gen_fonts.sh [iosevka|pragmata]" >&2
		exit 1
		;;
esac
mkdir -p "$OUT"

text_font() { # name size regular|bold
	local font="$REGULAR"
	[[ "$3" == bold ]] && font="$BOLD"
	"${CONV[@]}" --no-compress --no-prefilter --bpp "$BPP" --size "$2" --format lvgl --lv-include lvgl.h \
		--font "$font" -r "$TEXT" \
		${HRYVNIA[@]+"${HRYVNIA[@]}"} \
		--font "$AWESOME" -r "$SYMBOLS,$ICONS" \
		--force-fast-kern-format -o "$OUT/$1.c"
}

text_font font_small 14 regular
text_font font_label 14 bold
text_font font_body 15 regular
text_font font_title 16 bold
text_font font_large 20 bold

# Big digits for the clock and stopwatch (plus - . / : and space).
"${CONV[@]}" --no-compress --no-prefilter --bpp "$BPP" --size 40 --format lvgl --lv-include lvgl.h \
	--font "$BOLD" -r 0x2D-0x3A,0x20 \
	--force-fast-kern-format -o "$OUT/font_clock.c"

# Control hints (firmware/main/ui/hints.h): FontAwesome only, so one shared copy.
if [[ "$FAMILY" == iosevka ]]; then
	HINTS="61441,61634,61451,61452,61453,61457,61461,61473,61475,61478,61480,61512,61515,61516,61521,61523,61524,61536,61537,61589,61683,61761,62263"
	"${CONV[@]}" --no-compress --no-prefilter --bpp "$BPP" --size 10 --format lvgl --lv-include lvgl.h \
		--font "$AWESOME" -r "$HINTS" \
		--force-fast-kern-format -o "$OUT/font_hint.c"
fi

ls -la "$OUT"
