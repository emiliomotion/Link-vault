/*
 * ui_qwerty.h - Reusable on-device QWERTY keyboard widget.
 * Used by master password screen, recovery, edit dialogs, search, admin auth.
 */
#pragma once
#include <lvgl.h>
#include <functional>

namespace UI {
  struct QwertyCallbacks {
    std::function<void(char)>     onChar;     // a key character was typed
    std::function<void()>         onDelete;   // BACK pressed
    std::function<void()>         onOk;       // OK pressed
  };

  // Builds a QWERTY keyboard inside `parent`. Container's flex layout is
  // managed by the keyboard. Re-callable to reset / change callbacks.
  void buildQwerty(lv_obj_t* parent, const QwertyCallbacks& cbs);
}
