/*
 * theme.h - Cypherpunk amber-on-black theme constants
 */
#pragma once
#include <lvgl.h>

#define COLOR_BG       lv_color_hex(0x0A0A0A)
#define COLOR_FG       lv_color_hex(0xFFB000)   // amber
#define COLOR_DIM      lv_color_hex(0x663F00)
#define COLOR_ACCENT   lv_color_hex(0xFF4500)   // orange-red (warnings)
#define COLOR_ROW      lv_color_hex(0x151515)
#define COLOR_GREEN    lv_color_hex(0x00FF41)   // matrix green (OK)
#define COLOR_RED      lv_color_hex(0xFF2040)   // alarm red (decoy)

#define FONT_BODY      &lv_font_montserrat_14
#define FONT_SMALL     &lv_font_montserrat_12
#define FONT_LARGE     &lv_font_montserrat_28
