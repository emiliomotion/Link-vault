/*
 * ui_common.h - Reusable UI helpers (transitions, animations, toasts, etc.)
 */
#pragma once
#include <lvgl.h>
#include <functional>

namespace UI {
  // Wipe transition between screens. Calls lv_scr_load(next) at the apex.
  void wipeTransition(lv_obj_t* nextScreen);

  // Pulsing blink loop on opacity (for status dots, cursors)
  void startBlink(lv_obj_t* target);

  // Drifting horizontal scanline overlay
  void addScanline(lv_obj_t* parent);

  // Glitch-text effect: scrambles label characters then settles to finalText.
  void glitchLabel(lv_obj_t* label, const char* finalText, uint32_t totalMs);

  // Quick "key flash" feedback: bright background, fades back to default in 180ms.
  void flashKey(lv_obj_t* key);

  // Ephemeral toast at top of screen (auto-dismisses)
  void toast(const char* text, lv_color_t color);

  // In-device confirm dialog. Calls cb(true) on confirm, cb(false) on cancel.
  // Use this instead of any browser-style confirm.
  void confirmDialog(const char* title, const char* message,
                     const char* yesLabel, bool danger,
                     std::function<void(bool)> cb);

  // Long-press helper. Attaches a press timer to the object;
  // fires `cb` if held >= ms.
  void bindLongPress(lv_obj_t* obj, uint32_t ms, std::function<void()> cb);
}
