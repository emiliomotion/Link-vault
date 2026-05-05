/*
 * ui_edit.cpp - Full-screen editor overlays for links and categories
 */
#include "ui_edit.h"
#include "config.h"
#include "theme.h"
#include "ui_common.h"
#include "ui_qwerty.h"
#include "vaults.h"
#include "app_state.h"
#include <lvgl.h>
#include <vector>

extern void uiList_show();
extern void uiTransmit_url(const String& url);

namespace UIEdit {

// =========================================================================
//  EDIT LINK
// =========================================================================
struct EditLinkCtx {
  Link* link;
  String nameBuffer;
  String selectedCat;
  std::function<void()> onClose;
  lv_obj_t* overlay;
  lv_obj_t* nameDisplay;
  lv_obj_t* pillsContainer;
};
static EditLinkCtx* g_editCtx = nullptr;

static void renderLinkPills();
static void renderLinkName();
static void closeLinkEdit(bool deleted);
static void saveLinkEdit();

static void buildLinkPills() {
  if (!g_editCtx) return;
  lv_obj_clean(g_editCtx->pillsContainer);
  if (!App::currentVault) return;
  for (auto& c : App::currentVault->categories) {
    lv_obj_t* p = lv_btn_create(g_editCtx->pillsContainer);
    lv_obj_set_height(p, 18);
    lv_obj_set_style_bg_color(p, COLOR_BG, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_shadow_width(p, 0, 0);
    lv_obj_set_style_pad_hor(p, 6, 0);
    lv_obj_set_style_pad_ver(p, 0, 0);
    bool selected = (c.name == g_editCtx->selectedCat);
    lv_obj_set_style_border_width(p, 1, 0);
    lv_obj_set_style_border_color(p, selected ? lv_color_hex(c.color) : COLOR_DIM, 0);

    lv_obj_t* lab = lv_label_create(p);
    lv_label_set_text(lab, c.name.c_str());
    lv_obj_set_style_text_color(lab, selected ? lv_color_hex(c.color) : COLOR_DIM, 0);
    lv_obj_center(lab);

    String* heap = new String(c.name);
    lv_obj_set_user_data(p, heap);
    lv_obj_add_event_cb(p, [](lv_event_t* e){
      lv_obj_t* target = lv_event_get_target(e);
      String* n = (String*)lv_obj_get_user_data(target);
      if (lv_event_get_code(e) == LV_EVENT_DELETE) { delete n; return; }
      g_editCtx->selectedCat = *n;
      buildLinkPills();
    }, LV_EVENT_ALL, NULL);
  }

  // "none" pill
  lv_obj_t* none = lv_btn_create(g_editCtx->pillsContainer);
  lv_obj_set_height(none, 18);
  lv_obj_set_style_bg_color(none, COLOR_BG, 0);
  lv_obj_set_style_radius(none, 0, 0);
  lv_obj_set_style_shadow_width(none, 0, 0);
  lv_obj_set_style_pad_hor(none, 6, 0);
  lv_obj_set_style_pad_ver(none, 0, 0);
  bool noneSel = (g_editCtx->selectedCat.length() == 0);
  lv_obj_set_style_border_width(none, 1, 0);
  lv_obj_set_style_border_color(none, noneSel ? COLOR_FG : COLOR_DIM, 0);
  lv_obj_t* nl = lv_label_create(none);
  lv_label_set_text(nl, "none");
  lv_obj_set_style_text_color(nl, noneSel ? COLOR_FG : COLOR_DIM, 0);
  lv_obj_center(nl);
  lv_obj_add_event_cb(none, [](lv_event_t* e){
    g_editCtx->selectedCat = "";
    buildLinkPills();
  }, LV_EVENT_CLICKED, NULL);
}

static void renderLinkPills() { buildLinkPills(); }

static void renderLinkName() {
  if (!g_editCtx) return;
  lv_label_set_text(g_editCtx->nameDisplay, g_editCtx->nameBuffer.c_str());
}

static void closeLinkEdit(bool deleted) {
  if (!g_editCtx) return;
  auto cb = g_editCtx->onClose;
  lv_obj_del(g_editCtx->overlay);
  delete g_editCtx;
  g_editCtx = nullptr;
  if (cb) cb();
}

static void saveLinkEdit() {
  if (!g_editCtx) return;
  App::notifyActivity();
  String trimmed = g_editCtx->nameBuffer;
  trimmed.trim();
  if (trimmed.length() > 0) g_editCtx->link->name = trimmed;
  g_editCtx->link->cat = g_editCtx->selectedCat;
  Vaults::persist();
  closeLinkEdit(false);
}

void editLink(Link* link, std::function<void()> onClose) {
  if (g_editCtx) closeLinkEdit(false);
  g_editCtx = new EditLinkCtx();
  g_editCtx->link = link;
  g_editCtx->nameBuffer = link->name;
  g_editCtx->selectedCat = link->cat;
  g_editCtx->onClose = onClose;

  g_editCtx->overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g_editCtx->overlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(g_editCtx->overlay, 0, 0);
  lv_obj_set_style_bg_color(g_editCtx->overlay, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(g_editCtx->overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_editCtx->overlay, 0, 0);
  lv_obj_set_style_pad_all(g_editCtx->overlay, 4, 0);
  lv_obj_clear_flag(g_editCtx->overlay, LV_OBJ_FLAG_SCROLLABLE);

  // Title bar with CLOSE
  lv_obj_t* tb = lv_obj_create(g_editCtx->overlay);
  lv_obj_set_size(tb, SCREEN_W - 8, 16);
  lv_obj_set_pos(tb, 0, 0);
  lv_obj_set_style_bg_opa(tb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tb, 0, 0);
  lv_obj_set_style_pad_all(tb, 0, 0);
  lv_obj_clear_flag(tb, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(tb);
  String tt = "// EDIT :: " + link->name;
  lv_label_set_text(title, tt.c_str());
  lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t* close = lv_btn_create(tb);
  lv_obj_set_size(close, 70, 16);
  lv_obj_align(close, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(close, COLOR_BG, 0);
  lv_obj_set_style_border_width(close, 1, 0);
  lv_obj_set_style_border_color(close, COLOR_FG, 0);
  lv_obj_set_style_radius(close, 0, 0);
  lv_obj_set_style_shadow_width(close, 0, 0);
  lv_obj_t* cl = lv_label_create(close);
  lv_label_set_text(cl, "X CLOSE");
  lv_obj_set_style_text_color(cl, COLOR_FG, 0);
  lv_obj_center(cl);
  lv_obj_add_event_cb(close, [](lv_event_t* e){ closeLinkEdit(false); }, LV_EVENT_CLICKED, NULL);

  // Left column: URL display, name field, category pills
  lv_obj_t* left = lv_obj_create(g_editCtx->overlay);
  lv_obj_set_size(left, 240, 130);
  lv_obj_set_pos(left, 0, 18);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(left, 0, 0);
  lv_obj_set_style_pad_all(left, 2, 0);
  lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* urlLab = lv_label_create(left);
  lv_label_set_text(urlLab, "URL");
  lv_obj_set_style_text_color(urlLab, COLOR_DIM, 0);
  lv_obj_align(urlLab, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* urlDisplay = lv_label_create(left);
  lv_label_set_long_mode(urlDisplay, LV_LABEL_LONG_DOT);
  lv_obj_set_width(urlDisplay, 230);
  lv_label_set_text(urlDisplay, link->url.c_str());
  lv_obj_set_style_text_color(urlDisplay, COLOR_DIM, 0);
  lv_obj_align(urlDisplay, LV_ALIGN_TOP_LEFT, 0, 12);

  lv_obj_t* nameLab = lv_label_create(left);
  lv_label_set_text(nameLab, "NAME");
  lv_obj_set_style_text_color(nameLab, COLOR_DIM, 0);
  lv_obj_align(nameLab, LV_ALIGN_TOP_LEFT, 0, 30);

  g_editCtx->nameDisplay = lv_label_create(left);
  lv_obj_set_style_bg_color(g_editCtx->nameDisplay, COLOR_ROW, 0);
  lv_obj_set_style_bg_opa(g_editCtx->nameDisplay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_editCtx->nameDisplay, 1, 0);
  lv_obj_set_style_border_color(g_editCtx->nameDisplay, COLOR_ACCENT, 0);
  lv_obj_set_style_pad_hor(g_editCtx->nameDisplay, 4, 0);
  lv_obj_set_style_pad_ver(g_editCtx->nameDisplay, 1, 0);
  lv_obj_set_style_text_color(g_editCtx->nameDisplay, COLOR_FG, 0);
  lv_obj_set_width(g_editCtx->nameDisplay, 230);
  lv_label_set_text(g_editCtx->nameDisplay, g_editCtx->nameBuffer.c_str());
  lv_obj_align(g_editCtx->nameDisplay, LV_ALIGN_TOP_LEFT, 0, 42);

  lv_obj_t* catLab = lv_label_create(left);
  lv_label_set_text(catLab, "CATEGORY");
  lv_obj_set_style_text_color(catLab, COLOR_DIM, 0);
  lv_obj_align(catLab, LV_ALIGN_TOP_LEFT, 0, 64);

  g_editCtx->pillsContainer = lv_obj_create(left);
  lv_obj_set_size(g_editCtx->pillsContainer, 230, 40);
  lv_obj_set_pos(g_editCtx->pillsContainer, 0, 76);
  lv_obj_set_style_bg_opa(g_editCtx->pillsContainer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_editCtx->pillsContainer, 0, 0);
  lv_obj_set_style_pad_all(g_editCtx->pillsContainer, 0, 0);
  lv_obj_set_style_pad_gap(g_editCtx->pillsContainer, 2, 0);
  lv_obj_set_flex_flow(g_editCtx->pillsContainer, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_scroll_dir(g_editCtx->pillsContainer, LV_DIR_VER);
  buildLinkPills();

  // Right column: QWERTY keyboard
  lv_obj_t* kb = lv_obj_create(g_editCtx->overlay);
  lv_obj_set_size(kb, SCREEN_W - 248, 130);
  lv_obj_set_pos(kb, 244, 18);
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(kb, 0, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);

  UI::QwertyCallbacks cbs;
  cbs.onChar = [](char c){
    App::notifyActivity();
    g_editCtx->nameBuffer += c;
    renderLinkName();
  };
  cbs.onDelete = [](){
    App::notifyActivity();
    if (g_editCtx->nameBuffer.length() > 0) {
      g_editCtx->nameBuffer.remove(g_editCtx->nameBuffer.length() - 1);
      renderLinkName();
    }
  };
  cbs.onOk = [](){  // OK = SAVE
    saveLinkEdit();
  };
  UI::buildQwerty(kb, cbs);

  // Bottom button row: CANCEL / DELETE / TRANSMIT / SAVE
  const struct { const char* label; lv_color_t color; int x; int w; void(*cb)(lv_event_t*); } btns[] = {
    {"CANCEL", COLOR_FG, 0, 100, [](lv_event_t*){ closeLinkEdit(false); }},
    {"DELETE", COLOR_ACCENT, 102, 100, [](lv_event_t*){
      Link* l = g_editCtx->link;
      String name = l->name;
      UI::confirmDialog("// CONFIRM", ("Delete \"" + name + "\"?").c_str(), "DELETE", true,
        [l](bool ok){
          if (!ok) return;
          if (App::currentVault) {
            auto& links = App::currentVault->links;
            for (auto it = links.begin(); it != links.end(); ++it) {
              if (&(*it) == l) { links.erase(it); break; }
            }
            Vaults::persist();
          }
          closeLinkEdit(true);
        });
    }},
    {"TRANSMIT", COLOR_GREEN, 204, 100, [](lv_event_t*){
      String url = g_editCtx->link->url;
      closeLinkEdit(false);
      uiTransmit_url(url);
    }},
    {"SAVE", COLOR_GREEN, 306, 100, [](lv_event_t*){ saveLinkEdit(); }},
  };
  for (int i = 0; i < 4; i++) {
    lv_obj_t* b = lv_btn_create(g_editCtx->overlay);
    lv_obj_set_size(b, btns[i].w, 18);
    lv_obj_set_pos(b, btns[i].x, 152);
    lv_obj_set_style_bg_color(b, COLOR_BG, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, btns[i].color, 0);
    lv_obj_set_style_radius(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, btns[i].label);
    lv_obj_set_style_text_color(l, btns[i].color, 0);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, btns[i].cb, LV_EVENT_CLICKED, NULL);
  }
}

// =========================================================================
//  EDIT CATEGORY
// =========================================================================
struct EditCatCtx {
  Category* cat;
  bool isNew;
  String nameBuffer;
  std::function<void()> onClose;
  lv_obj_t* overlay;
  lv_obj_t* nameDisplay;
};
static EditCatCtx* g_catCtx = nullptr;

static void renderCatName() {
  if (!g_catCtx) return;
  lv_label_set_text(g_catCtx->nameDisplay, g_catCtx->nameBuffer.c_str());
}

static void closeCatEdit() {
  if (!g_catCtx) return;
  auto cb = g_catCtx->onClose;
  lv_obj_del(g_catCtx->overlay);
  delete g_catCtx;
  g_catCtx = nullptr;
  if (cb) cb();
}

static void doSaveCat() {
  if (!g_catCtx) return;
  App::notifyActivity();
  String n = g_catCtx->nameBuffer;
  n.trim(); n.toLowerCase();
  if (n.length() == 0) { closeCatEdit(); return; }

  if (g_catCtx->isNew) {
    if (!App::currentVault) { closeCatEdit(); return; }
    for (auto& c : App::currentVault->categories) {
      if (c.name == n) { UI::toast("category exists", COLOR_ACCENT); return; }
    }
    static const uint32_t COLORS[] = { 0xFFB000, 0x00FF41, 0xFF4500, 0xFF2040, 0x66DDFF, 0xCC88FF };
    Category nc;
    nc.name = n;
    nc.color = COLORS[App::currentVault->categories.size() % 6];
    App::currentVault->categories.push_back(nc);
    App::activeCategory = n;
  } else {
    String oldName = g_catCtx->cat->name;
    g_catCtx->cat->name = n;
    if (App::currentVault) {
      for (auto& l : App::currentVault->links) if (l.cat == oldName) l.cat = n;
    }
    if (App::activeCategory == oldName) App::activeCategory = n;
  }
  Vaults::persist();
  closeCatEdit();
}

void editCategory(Category* cat, bool isNew, std::function<void()> onClose) {
  if (g_catCtx) closeCatEdit();
  g_catCtx = new EditCatCtx();
  g_catCtx->cat = cat;
  g_catCtx->isNew = isNew;
  g_catCtx->nameBuffer = isNew ? "" : cat->name;
  g_catCtx->onClose = onClose;

  g_catCtx->overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g_catCtx->overlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(g_catCtx->overlay, 0, 0);
  lv_obj_set_style_bg_color(g_catCtx->overlay, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(g_catCtx->overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_catCtx->overlay, 0, 0);
  lv_obj_set_style_pad_all(g_catCtx->overlay, 4, 0);
  lv_obj_clear_flag(g_catCtx->overlay, LV_OBJ_FLAG_SCROLLABLE);

  // Title bar
  lv_obj_t* tb = lv_obj_create(g_catCtx->overlay);
  lv_obj_set_size(tb, SCREEN_W - 8, 16);
  lv_obj_set_pos(tb, 0, 0);
  lv_obj_set_style_bg_opa(tb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tb, 0, 0);
  lv_obj_set_style_pad_all(tb, 0, 0);
  lv_obj_clear_flag(tb, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(tb);
  lv_label_set_text(title, isNew ? "// NEW CATEGORY" : "// EDIT CATEGORY");
  lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t* close = lv_btn_create(tb);
  lv_obj_set_size(close, 70, 16);
  lv_obj_align(close, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(close, COLOR_BG, 0);
  lv_obj_set_style_border_width(close, 1, 0);
  lv_obj_set_style_border_color(close, COLOR_FG, 0);
  lv_obj_set_style_radius(close, 0, 0);
  lv_obj_set_style_shadow_width(close, 0, 0);
  lv_obj_t* cl = lv_label_create(close);
  lv_label_set_text(cl, "X CLOSE");
  lv_obj_set_style_text_color(cl, COLOR_FG, 0);
  lv_obj_center(cl);
  lv_obj_add_event_cb(close, [](lv_event_t* e){ closeCatEdit(); }, LV_EVENT_CLICKED, NULL);

  // Left: name field + info
  lv_obj_t* left = lv_obj_create(g_catCtx->overlay);
  lv_obj_set_size(left, 240, 130);
  lv_obj_set_pos(left, 0, 18);
  lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(left, 0, 0);
  lv_obj_set_style_pad_all(left, 2, 0);
  lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* nameLab = lv_label_create(left);
  lv_label_set_text(nameLab, "CATEGORY NAME");
  lv_obj_set_style_text_color(nameLab, COLOR_DIM, 0);
  lv_obj_align(nameLab, LV_ALIGN_TOP_LEFT, 0, 0);

  g_catCtx->nameDisplay = lv_label_create(left);
  lv_obj_set_style_bg_color(g_catCtx->nameDisplay, COLOR_ROW, 0);
  lv_obj_set_style_bg_opa(g_catCtx->nameDisplay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_catCtx->nameDisplay, 1, 0);
  lv_obj_set_style_border_color(g_catCtx->nameDisplay, COLOR_ACCENT, 0);
  lv_obj_set_style_pad_hor(g_catCtx->nameDisplay, 4, 0);
  lv_obj_set_style_pad_ver(g_catCtx->nameDisplay, 1, 0);
  lv_obj_set_style_text_color(g_catCtx->nameDisplay, COLOR_FG, 0);
  lv_obj_set_width(g_catCtx->nameDisplay, 230);
  lv_label_set_text(g_catCtx->nameDisplay, g_catCtx->nameBuffer.c_str());
  lv_obj_align(g_catCtx->nameDisplay, LV_ALIGN_TOP_LEFT, 0, 14);

  lv_obj_t* info = lv_label_create(left);
  lv_obj_set_style_text_color(info, COLOR_DIM, 0);
  lv_obj_set_width(info, 230);
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
  if (isNew) {
    lv_label_set_text(info, "type a name, then OK or SAVE");
  } else {
    int count = 0;
    if (App::currentVault) {
      for (auto& l : App::currentVault->links) if (l.cat == cat->name) count++;
    }
    String s = String(count) + " link" + (count == 1 ? "" : "s") + " use this category";
    lv_label_set_text(info, s.c_str());
  }
  lv_obj_align(info, LV_ALIGN_TOP_LEFT, 0, 40);

  // Right: QWERTY
  lv_obj_t* kb = lv_obj_create(g_catCtx->overlay);
  lv_obj_set_size(kb, SCREEN_W - 248, 130);
  lv_obj_set_pos(kb, 244, 18);
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(kb, 0, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);

  UI::QwertyCallbacks cbs;
  cbs.onChar = [](char c){
    App::notifyActivity();
    g_catCtx->nameBuffer += c;
    renderCatName();
  };
  cbs.onDelete = [](){
    App::notifyActivity();
    if (g_catCtx->nameBuffer.length() > 0) {
      g_catCtx->nameBuffer.remove(g_catCtx->nameBuffer.length() - 1);
      renderCatName();
    }
  };
  cbs.onOk = [](){
    doSaveCat();
  };
  UI::buildQwerty(kb, cbs);

  // Buttons: CANCEL / DELETE (only if !isNew) / SAVE
  lv_obj_t* cancel = lv_btn_create(g_catCtx->overlay);
  lv_obj_set_size(cancel, 100, 18);
  lv_obj_set_pos(cancel, 0, 152);
  lv_obj_set_style_bg_color(cancel, COLOR_BG, 0);
  lv_obj_set_style_border_width(cancel, 1, 0);
  lv_obj_set_style_border_color(cancel, COLOR_FG, 0);
  lv_obj_set_style_radius(cancel, 0, 0);
  lv_obj_set_style_shadow_width(cancel, 0, 0);
  lv_obj_t* cl2 = lv_label_create(cancel);
  lv_label_set_text(cl2, "CANCEL");
  lv_obj_set_style_text_color(cl2, COLOR_FG, 0);
  lv_obj_center(cl2);
  lv_obj_add_event_cb(cancel, [](lv_event_t*){ closeCatEdit(); }, LV_EVENT_CLICKED, NULL);

  if (!isNew) {
    lv_obj_t* del = lv_btn_create(g_catCtx->overlay);
    lv_obj_set_size(del, 100, 18);
    lv_obj_set_pos(del, 102, 152);
    lv_obj_set_style_bg_color(del, COLOR_BG, 0);
    lv_obj_set_style_border_width(del, 1, 0);
    lv_obj_set_style_border_color(del, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(del, 0, 0);
    lv_obj_set_style_shadow_width(del, 0, 0);
    lv_obj_t* dl = lv_label_create(del);
    lv_label_set_text(dl, "DELETE");
    lv_obj_set_style_text_color(dl, COLOR_ACCENT, 0);
    lv_obj_center(dl);
    lv_obj_add_event_cb(del, [](lv_event_t*){
      Category* c = g_catCtx->cat;
      String name = c->name;
      int count = 0;
      if (App::currentVault) {
        for (auto& l : App::currentVault->links) if (l.cat == name) count++;
      }
      String msg = count > 0
        ? "Delete category \"" + name + "\"? " + String(count) + " link(s) will become uncategorized."
        : "Delete category \"" + name + "\"?";
      UI::confirmDialog("// CONFIRM", msg.c_str(), "DELETE", true,
        [c, name](bool ok){
          if (!ok) return;
          if (App::currentVault) {
            auto& cats = App::currentVault->categories;
            for (auto it = cats.begin(); it != cats.end(); ++it) {
              if (&(*it) == c) { cats.erase(it); break; }
            }
            for (auto& l : App::currentVault->links) {
              if (l.cat == name) l.cat = "";
            }
            if (App::activeCategory == name) App::activeCategory = "ALL";
          }
          Vaults::persist();
          closeCatEdit();
        });
    }, LV_EVENT_CLICKED, NULL);
  }

  lv_obj_t* save = lv_btn_create(g_catCtx->overlay);
  lv_obj_set_size(save, 100, 18);
  lv_obj_set_pos(save, SCREEN_W - 108, 152);
  lv_obj_set_style_bg_color(save, COLOR_BG, 0);
  lv_obj_set_style_border_width(save, 1, 0);
  lv_obj_set_style_border_color(save, COLOR_GREEN, 0);
  lv_obj_set_style_radius(save, 0, 0);
  lv_obj_set_style_shadow_width(save, 0, 0);
  lv_obj_t* sl = lv_label_create(save);
  lv_label_set_text(sl, "SAVE");
  lv_obj_set_style_text_color(sl, COLOR_GREEN, 0);
  lv_obj_center(sl);
  lv_obj_add_event_cb(save, [](lv_event_t*){ doSaveCat(); }, LV_EVENT_CLICKED, NULL);
}

}  // namespace UIEdit
