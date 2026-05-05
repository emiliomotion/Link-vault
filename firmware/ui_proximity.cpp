/*
 * ui_proximity.cpp - "Awaiting trusted device" screen
 *
 * Shown after boot if a phone has been bonded but isn't currently in range.
 * Polls bleProximity_phonePresent() and transitions to lock screen when true.
 *
 * If proximity-override is enabled in settings, also shows a small "OVERRIDE"
 * button → master password screen → on success, sets proximityBypassed and
 * jumps to lock screen.
 */
#include <lvgl.h>
#include "config.h"
#include "theme.h"
#include "app_state.h"
#include "ui_common.h"
#include "storage.h"
#include "ui_qwerty.h"
#include "ble_proximity.h"

extern void uiLock_show();
extern void uiMasterAuthForOverride_show();   // defined in ui_lock.cpp

static lv_obj_t* scrProx;
static lv_obj_t* labelStatus;
static lv_timer_t* pollTimer = nullptr;

static void poll(lv_timer_t* t) {
  if (App::state != STATE_AWAIT_PROXIMITY) {
    if (pollTimer) { lv_timer_del(pollTimer); pollTimer = nullptr; }
    return;
  }
  if (bleProximity_phonePresent()) {
    if (pollTimer) { lv_timer_del(pollTimer); pollTimer = nullptr; }
    App::proximityOK = true;
    uiLock_show();  // uiLock_show sets state and handles the wipe transition
  }
}

void uiAwaitProximity_build() {
  scrProx = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scrProx, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scrProx, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scrProx, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* header = lv_label_create(scrProx);
  lv_label_set_text(header, "// LINK VAULT :: STANDBY");
  lv_obj_set_style_text_color(header, COLOR_DIM, 0);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 12, 8);

  labelStatus = lv_label_create(scrProx);
  lv_obj_set_style_text_color(labelStatus, COLOR_FG, 0);
  lv_obj_set_style_text_font(labelStatus, FONT_LARGE, 0);
  lv_label_set_text(labelStatus, "> AWAITING TRUSTED DEVICE");
  lv_obj_align(labelStatus, LV_ALIGN_LEFT_MID, 12, 0);

  lv_obj_t* hint = lv_label_create(scrProx);
  lv_obj_set_style_text_color(hint, COLOR_DIM, 0);
  lv_label_set_text(hint, "ensure paired phone is nearby with bluetooth on");
  lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 12, -8);

  // Pulsing dot
  lv_obj_t* dot = lv_obj_create(scrProx);
  lv_obj_set_size(dot, 12, 12);
  lv_obj_set_style_bg_color(dot, COLOR_ACCENT, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_radius(dot, 0, 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, -12, 8);
  UI::startBlink(dot);

  // Override button (only useful if override-enabled in settings)
  lv_obj_t* ov = lv_btn_create(scrProx);
  lv_obj_set_size(ov, 100, 24);
  lv_obj_align(ov, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
  lv_obj_set_style_bg_color(ov, COLOR_BG, 0);
  lv_obj_set_style_border_width(ov, 1, 0);
  lv_obj_set_style_border_color(ov, COLOR_DIM, 0);
  lv_obj_set_style_radius(ov, 0, 0);
  lv_obj_set_style_shadow_width(ov, 0, 0);
  lv_obj_t* ol = lv_label_create(ov);
  lv_label_set_text(ol, "OVERRIDE");
  lv_obj_set_style_text_color(ol, COLOR_DIM, 0);
  lv_obj_center(ol);
  lv_obj_add_event_cb(ov, [](lv_event_t* e){
    GlobalSettings g = Storage::loadGlobalSettings();
    if (!g.proximityOverrideEnabled) {
      UI::toast("override disabled in admin", COLOR_ACCENT);
      return;
    }
    uiMasterAuthForOverride_show();
  }, LV_EVENT_CLICKED, NULL);

  UI::addScanline(scrProx);
}

void uiAwaitProximity_show() {
  if (!scrProx) uiAwaitProximity_build();

  // If no phone bonded yet (first boot), skip directly to lock screen.
  // The user will set up bonding from admin once they're in.
  if (bleProximity_bondedList().empty()) {
    App::state = STATE_LOCK;
    uiLock_show();
    return;
  }

  // If already in range, skip directly
  if (bleProximity_phonePresent()) {
    App::proximityOK = true;
    App::state = STATE_LOCK;
    uiLock_show();
    return;
  }

  App::state = STATE_AWAIT_PROXIMITY;
  App::proximityOK = false;
  lv_scr_load(scrProx);

  if (pollTimer) lv_timer_del(pollTimer);
  pollTimer = lv_timer_create(poll, 500, nullptr);
}
