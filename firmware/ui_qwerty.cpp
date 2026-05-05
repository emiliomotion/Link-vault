/*
 * ui_qwerty.cpp
 */
#include "ui_qwerty.h"
#include "ui_common.h"
#include "theme.h"
#include "config.h"
#include <vector>

namespace UI {

enum KbMode { MODE_LOWER, MODE_UPPER, MODE_SYM };

struct KbState {
  lv_obj_t* parent;
  KbMode mode;
  QwertyCallbacks cbs;
};

static const char* ROWS_LOWER[] = {
  "1234567890",
  "qwertyuiop",
  "asdfghjkl",
  "*zxcvbnm<",         // * = SHIFT, < = BACK
  "%>$"                // % = SYM, > = SPACE, $ = OK
};
static const char* ROWS_UPPER[] = {
  "1234567890",
  "QWERTYUIOP",
  "ASDFGHJKL",
  "*ZXCVBNM<",
  "%>$"
};
static const char* ROWS_SYM[] = {
  "1234567890",
  "!@#$%^&*()",
  "-_+=:;.,?",
  "*/\\\"'[]{}<",
  "@>$"               // @ = ABC switch back
};

static void renderKeyboard(KbState* s);

static void onKeyPress(lv_event_t* e) {
  KbState* s = (KbState*)lv_event_get_user_data(e);
  lv_obj_t* btn = lv_event_get_target(e);
  char k = (char)(intptr_t)lv_obj_get_user_data(btn);
  if (!k) return;

  flashKey(btn);

  switch (k) {
    case '*':  // SHIFT
      s->mode = (s->mode == MODE_UPPER) ? MODE_LOWER : MODE_UPPER;
      renderKeyboard(s);
      return;
    case '%':  // SYM
      s->mode = MODE_SYM;
      renderKeyboard(s);
      return;
    case '@':  // ABC (back from sym)
      s->mode = MODE_LOWER;
      renderKeyboard(s);
      return;
    case '<':  // BACK
      if (s->cbs.onDelete) s->cbs.onDelete();
      return;
    case '$':  // OK
      if (s->cbs.onOk) s->cbs.onOk();
      return;
    case '>':  // SPACE
      if (s->cbs.onChar) s->cbs.onChar(' ');
      return;
    default:
      if (s->cbs.onChar) s->cbs.onChar(k);
      return;
  }
}

static void renderKeyboard(KbState* s) {
  lv_obj_clean(s->parent);
  lv_obj_set_flex_flow(s->parent, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(s->parent, 1, 0);
  lv_obj_set_style_pad_gap(s->parent, 2, 0);

  const char** rows;
  switch (s->mode) {
    case MODE_UPPER: rows = ROWS_UPPER; break;
    case MODE_SYM:   rows = ROWS_SYM;   break;
    default:         rows = ROWS_LOWER; break;
  }

  for (int r = 0; r < 5; r++) {
    lv_obj_t* row = lv_obj_create(s->parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 2, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const char* cells = rows[r];
    int n = strlen(cells);
    for (int i = 0; i < n; i++) {
      char c = cells[i];
      lv_obj_t* btn = lv_btn_create(row);
      lv_obj_set_height(btn, 22);

      // Determine width / styling
      bool isSpecial = (c == '*' || c == '<' || c == '%' || c == '@' || c == '$');
      bool isSpace   = (c == '>');
      if (isSpace)        lv_obj_set_flex_grow(btn, 4);
      else if (isSpecial) lv_obj_set_flex_grow(btn, 2);
      else                lv_obj_set_flex_grow(btn, 1);

      lv_obj_set_style_bg_color(btn, COLOR_ROW, 0);
      lv_obj_set_style_border_width(btn, 1, 0);
      lv_obj_set_style_border_color(btn, COLOR_DIM, 0);
      lv_obj_set_style_radius(btn, 0, 0);
      lv_obj_set_style_shadow_width(btn, 0, 0);
      lv_obj_set_style_pad_all(btn, 0, 0);

      lv_obj_t* lab = lv_label_create(btn);
      const char* labelText;
      switch (c) {
        case '*': labelText = "SHIFT"; break;
        case '<': labelText = "BACK"; break;
        case '%': labelText = "SYM"; break;
        case '@': labelText = "ABC"; break;
        case '$': labelText = "OK"; break;
        case '>': labelText = "___"; break;
        default: {
          static char buf[2] = {0,0};
          buf[0] = c;
          labelText = buf;
          break;
        }
      }
      lv_label_set_text(lab, labelText);
      lv_obj_center(lab);

      // Color: OK = green, SHIFT/SYM/ABC = orange-red, normal = amber
      if (c == '$') {
        lv_obj_set_style_text_color(lab, COLOR_GREEN, 0);
        lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
      } else if (c == '*' || c == '%' || c == '@') {
        lv_obj_set_style_text_color(lab, COLOR_ACCENT, 0);
      } else {
        lv_obj_set_style_text_color(lab, COLOR_FG, 0);
      }

      // Store character as integer cast — avoids heap allocation per key.
      lv_obj_set_user_data(btn, (void*)(intptr_t)c);

      lv_obj_add_event_cb(btn, onKeyPress, LV_EVENT_CLICKED, s);
    }
  }
}

void buildQwerty(lv_obj_t* parent, const QwertyCallbacks& cbs) {
  // Reuse a KbState bound to the parent (one per parent).
  KbState* s = (KbState*)lv_obj_get_user_data(parent);
  if (!s) {
    s = new KbState();
    s->parent = parent;
    lv_obj_set_user_data(parent, s);
  }
  s->cbs = cbs;
  s->mode = MODE_LOWER;
  renderKeyboard(s);
}

}  // namespace UI
