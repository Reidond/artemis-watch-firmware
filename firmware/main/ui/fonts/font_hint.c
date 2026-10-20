/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: --no-compress --no-prefilter --bpp 1 --size 10 --format lvgl --lv-include lvgl.h --font firmware/managed_components/lvgl__lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff -r 61441,61634,61451,61452,61453,61457,61461,61473,61475,61478,61480,61512,61515,61516,61521,61523,61524,61536,61537,61589,61683,61761,62263 --force-fast-kern-format -o firmware/main/ui/fonts/font_hint.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef FONT_HINT
#define FONT_HINT 1
#endif

#if FONT_HINT

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F001 "" */
    0x0, 0x40, 0xf1, 0xfc, 0x7d, 0x18, 0x44, 0x11,
    0x4, 0x47, 0x71, 0xdc, 0x0,

    /* U+F00B "" */
    0xef, 0xfb, 0xf0, 0x3, 0xbf, 0xef, 0xc0, 0xe,
    0xff, 0xbf,

    /* U+F00C "" */
    0x0, 0xc0, 0x64, 0x33, 0x98, 0x7c, 0xe, 0x1,
    0x0,

    /* U+F00D "" */
    0xc7, 0xdd, 0xf1, 0xc7, 0xdd, 0xf1, 0x80,

    /* U+F011 "" */
    0xc, 0xb, 0x46, 0xdb, 0x33, 0xcc, 0xf0, 0x3c,
    0x1d, 0x86, 0x3f, 0x0,

    /* U+F015 "" */
    0x4, 0x83, 0x70, 0xd6, 0x37, 0x6d, 0xf6, 0x7f,
    0xe, 0xe1, 0xdc, 0x3b, 0x80,

    /* U+F021 "" */
    0x0, 0x47, 0xd6, 0x1d, 0xf, 0x0, 0x0, 0xf,
    0xb, 0x6, 0xe3, 0x27, 0x80,

    /* U+F023 "" */
    0x1c, 0x11, 0x18, 0xcc, 0x6f, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x80,

    /* U+F026 "" */
    0x8, 0xff, 0xff, 0xfc, 0x61,

    /* U+F028 "" */
    0x8, 0x83, 0x2b, 0xe2, 0xfc, 0xdf, 0x9b, 0xf5,
    0x46, 0x70, 0x42, 0x0, 0x80,

    /* U+F048 "" */
    0x86, 0x7b, 0xff, 0xff, 0xfb, 0xe7, 0x84,

    /* U+F04B "" */
    0xc0, 0x70, 0x3e, 0x1f, 0xcf, 0xff, 0xff, 0xfd,
    0xf8, 0xf0, 0x70, 0x0,

    /* U+F04C "" */
    0xf7, 0xfb, 0xfd, 0xfe, 0xff, 0x7f, 0xbf, 0xdf,
    0xef, 0xf7, 0x80,

    /* U+F051 "" */
    0x87, 0x9f, 0x7f, 0xff, 0xff, 0x79, 0x84,

    /* U+F053 "" */
    0x19, 0x99, 0x8c, 0x30, 0xc3,

    /* U+F054 "" */
    0x86, 0x18, 0x63, 0xb3, 0x10,

    /* U+F060 "" */
    0x18, 0x18, 0x18, 0x1f, 0xf6, 0x1, 0x80, 0x60,

    /* U+F061 "" */
    0xc, 0x3, 0x0, 0xdf, 0xf0, 0x30, 0x30, 0x30,

    /* U+F095 "" */
    0x1, 0x80, 0x70, 0x3c, 0x7, 0x1, 0x80, 0x62,
    0x33, 0xf8, 0xfc, 0x3e, 0x0,

    /* U+F0C2 "" */
    0x1e, 0x1, 0xfe, 0xf, 0xf8, 0xff, 0xcf, 0xff,
    0xff, 0xff, 0xff, 0xef, 0xfe,

    /* U+F0F3 "" */
    0x8, 0xe, 0xf, 0x8f, 0xe7, 0xf3, 0xf9, 0xfd,
    0xff, 0x0, 0xe, 0x0,

    /* U+F141 "" */
    0xec, 0xfb, 0xfe, 0xcc,

    /* U+F337 "" */
    0x61, 0xbf, 0xf6, 0x18
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 13, .adv_w = 160, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 32, .adv_w = 110, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 39, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 51, .adv_w = 180, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 64, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 77, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 88, .adv_w = 80, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 93, .adv_w = 180, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 106, .adv_w = 140, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 113, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 125, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 136, .adv_w = 140, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 143, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 148, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 140, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 161, .adv_w = 140, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 169, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 182, .adv_w = 200, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 195, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 207, .adv_w = 160, .box_w = 10, .box_h = 3, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 211, .adv_w = 160, .box_w = 10, .box_h = 3, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0xa, 0xb, 0xc, 0x10, 0x14, 0x20, 0x22,
    0x25, 0x27, 0x47, 0x4a, 0x4b, 0x50, 0x52, 0x53,
    0x5f, 0x60, 0x94, 0xc1, 0xf2, 0x140, 0x336
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 61441, .range_length = 823, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 23, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font_hint = {
#else
lv_font_t font_hint = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 10,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -4,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FONT_HINT*/

