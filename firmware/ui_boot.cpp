/*
 * ui_boot.cpp - Boot sequence with terminal log animation
 */
#include <lvgl.h>
#include <Arduino.h>
#include "config.h"
#include "theme.h"
#include "ui_common.h"
#include "app_state.h"

extern void uiAwaitProximity_show();

static lv_obj_t* scrBoot;
static lv_obj_t* bootLog;
static lv_obj_t* bootTitle;
static uint8_t   bootStep = 0;
static String    bootAccumulated = "";
static lv_timer_t* bootTimer = nullptr;

static const char* BOOT_LINES[] = {
  "[ BOOT ] initializing subsystems",
  "[  OK  ] kernel ............ loaded",
  "[  OK  ] storage ........... mounted",
  "[  OK  ] crypto ............ ready",
  "[  OK  ] bluetooth HID ..... online",
  "[  OK  ] vaults ............ loaded",
  "",
  "> awaiting credentials_"
};
#define BOOT_LINE_COUNT (sizeof(BOOT_LINES) / sizeof(BOOT_LINES[0]))

static void bootTickCb(lv_timer_t* t);

void uiBoot_build() {
  scrBoot = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scrBoot, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scrBoot, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scrBoot, 0, 0);
  lv_obj_clear_flag(scrBoot, LV_OBJ_FLAG_SCROLLABLE);

  bootTitle = lv_label_create(scrBoot);
  lv_obj_set_style_text_font(bootTitle, FONT_LARGE, 0);
  lv_obj_set_style_text_color(bootTitle, COLOR_FG, 0);
  lv_label_set_text(bootTitle, "");
  lv_obj_align(bootTitle, LV_ALIGN_LEFT_MID, 16, -20);

  lv_obj_t* sub = lv_label_create(scrBoot);
  lv_obj_set_style_text_color(sub, COLOR_DIM, 0);
  lv_label_set_text(sub, "v1.0 :: secure uri cache");
  lv_obj_align_to(sub, bootTitle, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);

  bootLog = lv_label_create(scrBoot);
  lv_obj_set_style_text_color(bootLog, COLOR_FG, 0);
  lv_label_set_recolor(bootLog, true);
  lv_label_set_text(bootLog, "");
  lv_obj_set_width(bootLog, 340);
  lv_obj_align(bootLog, LV_ALIGN_RIGHT_MID, -12, 0);

  UI::addScanline(scrBoot);
}

void uiBoot_show() {
  lv_scr_load(scrBoot);
  bootStep = 0;
  bootAccumulated = "";
  lv_label_set_text(bootLog, "");
  lv_label_set_text(bootTitle, "");

  // Glitch the title in
  lv_timer_t* t1 = lv_timer_create([](lv_timer_t* timer){
    UI::glitchLabel(bootTitle, "// LINK VAULT", 600);
    lv_timer_del(timer);
  }, 200, nullptr);
  lv_timer_set_repeat_count(t1, 1);

  // Start log printing after title settles
  lv_timer_t* t2 = lv_timer_create([](lv_timer_t* timer){
    bootTimer = lv_timer_create(bootTickCb, 220, nullptr);
    lv_timer_del(timer);
  }, 1100, nullptr);
  lv_timer_set_repeat_count(t2, 1);
}

static void bootTickCb(lv_timer_t* t) {
  if (bootStep >= BOOT_LINE_COUNT) {
    lv_timer_del(bootTimer);
    bootTimer = nullptr;

    // Hold briefly, then transition to "awaiting proximity" or directly to lock
    lv_timer_t* nextT = lv_timer_create([](lv_timer_t* timer){
      uiAwaitProximity_show();
      lv_timer_del(timer);
    }, 700, nullptr);
    lv_timer_set_repeat_count(nextT, 1);
    return;
  }

  if (bootAccumulated.length() > 0) bootAccumulated += "\n";
  bootAccumulated += BOOT_LINES[bootStep];

  // Recolor markers
  String colored = "";
  int start = 0;
  for (int i = 0; i <= (int)bootAccumulated.length(); i++) {
    if (i == (int)bootAccumulated.length() || bootAccumulated[i] == '\n') {
      String ln = bootAccumulated.substring(start, i);
      ln.replace("[  OK  ]", "#00ff41 [OK]#");
      ln.replace("[ BOOT ]", "#ffb000 [>>]#");
      if (ln.length() > 0 && ln[0] == '>') ln = "#ff4500 " + ln + "#";
      colored += ln;
      if (i < (int)bootAccumulated.length()) colored += "\n";
      start = i + 1;
    }
  }
  lv_label_set_text(bootLog, colored.c_str());

  bootStep++;
}
