/*
 * ui_common.cpp
 */
#include "ui_common.h"
#include "config.h"
#include "theme.h"
#include <Arduino.h>
#include <functional>
#include <vector>

namespace UI {

// =========================================================================
//  WIPE TRANSITION
// =========================================================================
struct WipeCtx {
  lv_obj_t* wipe;
  lv_obj_t* next;
};

static void wipePhase1Done(lv_anim_t* anim) {
  WipeCtx* ctx = (WipeCtx*)anim->user_data;
  // Apex: load the next screen now that the wipe covers everything
  lv_scr_load(ctx->next);

  // Phase 2: slide off to the right
  lv_anim_t a2;
  lv_anim_init(&a2);
  lv_anim_set_var(&a2, ctx->wipe);
  lv_anim_set_values(&a2, 0, SCREEN_W);
  lv_anim_set_time(&a2, WIPE_TRANSITION_MS);
  lv_anim_set_path_cb(&a2, lv_anim_path_ease_in);
  lv_anim_set_exec_cb(&a2, [](void* obj, int32_t x){
    lv_obj_set_x((lv_obj_t*)obj, x);
  });
  static auto cleanup = [](lv_anim_t* a){
    WipeCtx* c = (WipeCtx*)a->user_data;
    lv_obj_del(c->wipe);
    delete c;
  };
  lv_anim_set_user_data(&a2, ctx);
  lv_anim_set_ready_cb(&a2, cleanup);
  lv_anim_start(&a2);
}

void wipeTransition(lv_obj_t* nextScreen) {
  WipeCtx* ctx = new WipeCtx();
  ctx->next = nextScreen;
  ctx->wipe = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ctx->wipe, 0, SCREEN_H);
  lv_obj_set_pos(ctx->wipe, 0, 0);
  lv_obj_set_style_bg_color(ctx->wipe, COLOR_FG, 0);
  lv_obj_set_style_bg_opa(ctx->wipe, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ctx->wipe, 0, 0);
  lv_obj_set_style_radius(ctx->wipe, 0, 0);
  lv_obj_clear_flag(ctx->wipe, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ctx->wipe);
  lv_anim_set_values(&a, 0, SCREEN_W);
  lv_anim_set_time(&a, WIPE_TRANSITION_MS);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t w){
    lv_obj_set_width((lv_obj_t*)obj, w);
  });
  lv_anim_set_user_data(&a, ctx);
  lv_anim_set_ready_cb(&a, wipePhase1Done);
  lv_anim_start(&a);
}

// =========================================================================
//  BLINK
// =========================================================================
void startBlink(lv_obj_t* target) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, target);
  lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
  lv_anim_set_time(&a, 900);
  lv_anim_set_playback_time(&a, 900);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v){
    lv_obj_set_style_bg_opa((lv_obj_t*)obj, (lv_opa_t)v, 0);
  });
  lv_anim_start(&a);
}

// =========================================================================
//  SCANLINE
// =========================================================================
void addScanline(lv_obj_t* parent) {
  lv_obj_t* line = lv_obj_create(parent);
  lv_obj_set_size(line, SCREEN_W, 1);
  lv_obj_set_style_bg_color(line, COLOR_FG, 0);
  lv_obj_set_style_bg_opa(line, LV_OPA_20, 0);
  lv_obj_set_style_border_width(line, 0, 0);
  lv_obj_set_style_radius(line, 0, 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(line, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_pos(line, 0, -2);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, line);
  lv_anim_set_values(&a, -2, SCREEN_H + 2);
  lv_anim_set_time(&a, 4200);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t y){
    lv_obj_set_y((lv_obj_t*)obj, y);
  });
  lv_anim_start(&a);
}

// =========================================================================
//  GLITCH LABEL
// =========================================================================
struct GlitchCtx {
  lv_obj_t* label;
  String finalText;
  uint32_t start;
  uint32_t duration;
  lv_timer_t* timer;  // kept so the label's DELETE handler can cancel it
};

static const char GLITCH_CHARS[] = "!@#$%^&*<>/\\|{}[]=-+?";

void glitchLabel(lv_obj_t* label, const char* finalText, uint32_t totalMs) {
  GlitchCtx* ctx = new GlitchCtx();
  ctx->label = label;
  ctx->finalText = finalText;
  ctx->start = millis();
  ctx->duration = totalMs;
  ctx->timer = nullptr;

  ctx->timer = lv_timer_create([](lv_timer_t* t){
    GlitchCtx* c = (GlitchCtx*)t->user_data;
    uint32_t elapsed = millis() - c->start;
    if (elapsed >= c->duration) {
      lv_label_set_text(c->label, c->finalText.c_str());
      c->timer = nullptr;
      delete c;
      lv_timer_del(t);
      return;
    }
    String s = c->finalText;
    int n = 1 + (esp_random() % 3);
    for (int i = 0; i < n && s.length() > 0; i++) {
      int idx = esp_random() % s.length();
      char ch = s[idx];
      if (ch != ' ' && ch != '/' && ch != '\n') {
        s[idx] = GLITCH_CHARS[esp_random() % (sizeof(GLITCH_CHARS) - 1)];
      }
    }
    lv_label_set_text(c->label, s.c_str());
  }, 60, ctx);

  // If the label is deleted while the glitch is still running, cancel the timer.
  lv_obj_add_event_cb(label, [](lv_event_t* e){
    GlitchCtx* c = (GlitchCtx*)lv_event_get_user_data(e);
    if (c && c->timer) { lv_timer_del(c->timer); c->timer = nullptr; }
    delete c;
  }, LV_EVENT_DELETE, ctx);
}

// =========================================================================
//  KEY FLASH
// =========================================================================
struct FlashCtx {
  lv_obj_t* key;
  lv_color_t origBg;
  lv_color_t origText;
  lv_color_t origBorder;
};

void flashKey(lv_obj_t* key) {
  // Capture the *current* style — works whether the key is "normal" or "OK".
  FlashCtx* ctx = new FlashCtx();
  ctx->key = key;
  ctx->origBg     = lv_obj_get_style_bg_color(key, 0);
  ctx->origText   = lv_obj_get_style_text_color(key, 0);
  ctx->origBorder = lv_obj_get_style_border_color(key, 0);

  // Apply bright state immediately
  lv_obj_set_style_bg_color(key, COLOR_FG, 0);
  // Let the LVGL transition animate back smoothly
  static const lv_style_prop_t props[] = {
    LV_STYLE_BG_COLOR, LV_STYLE_TEXT_COLOR, LV_STYLE_BORDER_COLOR, (lv_style_prop_t)0
  };
  static lv_style_transition_dsc_t trans;
  static bool transInit = false;
  if (!transInit) {
    lv_style_transition_dsc_init(&trans, props, lv_anim_path_ease_out, KEY_FLASH_MS, 0, NULL);
    transInit = true;
  }
  lv_obj_set_style_transition(key, &trans, 0);

  // Schedule the revert so the transition has something to animate back to
  lv_timer_t* t = lv_timer_create([](lv_timer_t* timer){
    FlashCtx* c = (FlashCtx*)timer->user_data;
    lv_obj_set_style_bg_color(c->key, c->origBg, 0);
    lv_obj_set_style_text_color(c->key, c->origText, 0);
    lv_obj_set_style_border_color(c->key, c->origBorder, 0);
    delete c;
    lv_timer_del(timer);
  }, 30, ctx);
  lv_timer_set_repeat_count(t, 1);
}

// =========================================================================
//  TOAST
// =========================================================================
void toast(const char* text, lv_color_t color) {
  lv_obj_t* t = lv_label_create(lv_layer_top());
  lv_label_set_text(t, text);
  lv_obj_set_style_text_color(t, color, 0);
  lv_obj_set_style_bg_color(t, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(t, 6, 0);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_border_color(t, color, 0);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);

  lv_timer_t* tm = lv_timer_create([](lv_timer_t* timer){
    lv_obj_del((lv_obj_t*)timer->user_data);
    lv_timer_del(timer);
  }, TOAST_MS, t);
  lv_timer_set_repeat_count(tm, 1);
}

// =========================================================================
//  CONFIRM DIALOG
// =========================================================================
struct ConfirmCtx {
  lv_obj_t* overlay;
  std::function<void(bool)> cb;
};

static void closeConfirm(ConfirmCtx* ctx, bool result) {
  auto cb = ctx->cb;
  lv_obj_t* ov = ctx->overlay;
  ctx->overlay = nullptr;  // prevent DELETE handler from double-freeing
  delete ctx;
  if (ov) lv_obj_del(ov);
  if (cb) cb(result);
}

void confirmDialog(const char* title, const char* message,
                   const char* yesLabel, bool danger,
                   std::function<void(bool)> cb) {
  ConfirmCtx* ctx = new ConfirmCtx();
  ctx->cb = cb;
  ctx->overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ctx->overlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(ctx->overlay, 0, 0);
  lv_obj_set_style_bg_color(ctx->overlay, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(ctx->overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ctx->overlay, 1, 0);
  lv_obj_set_style_border_color(ctx->overlay, danger ? COLOR_ACCENT : COLOR_FG, 0);
  lv_obj_set_style_radius(ctx->overlay, 0, 0);
  lv_obj_set_style_pad_all(ctx->overlay, 8, 0);
  lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_SCROLLABLE);

  // Free ctx if overlay is deleted externally (e.g., screen transition).
  lv_obj_add_event_cb(ctx->overlay, [](lv_event_t* e){
    auto c = (ConfirmCtx*)lv_event_get_user_data(e);
    // Nullify overlay pointer so closeConfirm doesn't double-free.
    if (c) { c->overlay = nullptr; delete c; }
  }, LV_EVENT_DELETE, ctx);

  lv_obj_t* tlbl = lv_label_create(ctx->overlay);
  lv_label_set_text(tlbl, title);
  lv_obj_set_style_text_color(tlbl, danger ? COLOR_ACCENT : COLOR_FG, 0);
  lv_obj_align(tlbl, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* msg = lv_label_create(ctx->overlay);
  lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(msg, SCREEN_W - 32);
  lv_label_set_text(msg, message);
  lv_obj_set_style_text_color(msg, COLOR_FG, 0);
  lv_obj_align(msg, LV_ALIGN_LEFT_MID, 0, -4);

  // Buttons
  lv_obj_t* cancel = lv_btn_create(ctx->overlay);
  lv_obj_set_size(cancel, 100, 28);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(cancel, COLOR_BG, 0);
  lv_obj_set_style_border_width(cancel, 1, 0);
  lv_obj_set_style_border_color(cancel, COLOR_FG, 0);
  lv_obj_set_style_radius(cancel, 0, 0);
  lv_obj_set_style_shadow_width(cancel, 0, 0);
  lv_obj_t* cl = lv_label_create(cancel);
  lv_label_set_text(cl, "CANCEL");
  lv_obj_set_style_text_color(cl, COLOR_FG, 0);
  lv_obj_center(cl);
  lv_obj_add_event_cb(cancel, [](lv_event_t* e){
    auto c = (ConfirmCtx*)lv_event_get_user_data(e);
    closeConfirm(c, false);
  }, LV_EVENT_CLICKED, ctx);

  lv_obj_t* ok = lv_btn_create(ctx->overlay);
  lv_obj_set_size(ok, 140, 28);
  lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(ok, COLOR_BG, 0);
  lv_obj_set_style_border_width(ok, 1, 0);
  lv_obj_set_style_border_color(ok, danger ? COLOR_ACCENT : COLOR_GREEN, 0);
  lv_obj_set_style_radius(ok, 0, 0);
  lv_obj_set_style_shadow_width(ok, 0, 0);
  lv_obj_t* ol = lv_label_create(ok);
  lv_label_set_text(ol, yesLabel);
  lv_obj_set_style_text_color(ol, danger ? COLOR_ACCENT : COLOR_GREEN, 0);
  lv_obj_center(ol);
  lv_obj_add_event_cb(ok, [](lv_event_t* e){
    auto c = (ConfirmCtx*)lv_event_get_user_data(e);
    closeConfirm(c, true);
  }, LV_EVENT_CLICKED, ctx);
}

// =========================================================================
//  LONG PRESS
// =========================================================================
struct LongPressCtx {
  uint32_t ms;
  std::function<void()> cb;
  uint32_t pressedAt;
  bool firing;
};

void bindLongPress(lv_obj_t* obj, uint32_t ms, std::function<void()> cb) {
  LongPressCtx* ctx = new LongPressCtx();
  ctx->ms = ms;
  ctx->cb = cb;
  ctx->pressedAt = 0;
  ctx->firing = false;

  lv_obj_add_event_cb(obj, [](lv_event_t* e){
    auto c = (LongPressCtx*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
      delete c;
      return;
    }
    if (code == LV_EVENT_PRESSED) {
      c->pressedAt = millis();
      c->firing = false;
    } else if (code == LV_EVENT_PRESSING) {
      if (!c->firing && c->pressedAt && (millis() - c->pressedAt >= c->ms)) {
        c->firing = true;
        if (c->cb) c->cb();
      }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
      c->pressedAt = 0;
    }
  }, LV_EVENT_ALL, ctx);
}

}  // namespace UI
