/*
 * ui_list.cpp - Main list screen (after unlock)
 *
 * Layout (640x172):
 *   [22px topbar: vault name | BLE dot | batt | SRCH | CFG | LOCK]
 *   [20px tab strip:  all | daily | reading | tools | + ]
 *   [remaining: scrollable rows of links]
 *
 * Long-press row → edit overlay.
 * Tap pill on row → filter by that category.
 * Long-press tab → manage that category.
 * Tap '+' tab → create new category.
 * SRCH button → full-screen search overlay.
 */
#include <lvgl.h>
#include <BleKeyboard.h>
#include "config.h"
#include "theme.h"
#include "app_state.h"
#include "ui_common.h"
#include "ui_qwerty.h"
#include "ui_edit.h"
#include "vaults.h"
#include "security.h"

extern BleKeyboard bleKeyboard;
extern void uiConfig_show();

// =========================================================================
//  STATE
// =========================================================================
static lv_obj_t* scrList;
static lv_obj_t* listTitle;
static lv_obj_t* bleDot;
static lv_obj_t* battLabel;
static lv_obj_t* tabStrip;
static lv_obj_t* rowsContainer;
static lv_obj_t* tamperLabel;

// Search overlay
static lv_obj_t* searchOverlay = nullptr;
static lv_obj_t* searchInput = nullptr;
static lv_obj_t* searchResults = nullptr;
static String searchBuffer = "";

static void buildList();
static void renderTopbar();
static void renderTabStrip();
static void renderRows();
static void openSearch();
static void closeSearch();
static void renderSearchResults();
static void buildSearchOverlay();
extern int batteryRead();   // returns 0-100 (defined in config UI)

void uiTransmit_url(const String& url);

// =========================================================================
//  ENTRY POINT
// =========================================================================
void uiList_build() {
  buildList();
}

void uiList_show() {
  if (!App::currentVault) return;
  // Reload UI in case categories/links changed in edit overlay
  renderTopbar();
  renderTabStrip();
  renderRows();
  if (tamperLabel) {
    lv_label_set_text(tamperLabel, Security::tamperReport().c_str());
  }
  UI::wipeTransition(scrList);
}

// =========================================================================
//  BUILD
// =========================================================================
static void buildList() {
  scrList = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scrList, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scrList, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scrList, 0, 0);
  lv_obj_clear_flag(scrList, LV_OBJ_FLAG_SCROLLABLE);

  // ----- TOP BAR -----
  lv_obj_t* bar = lv_obj_create(scrList);
  lv_obj_set_size(bar, SCREEN_W, 22);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, COLOR_BG, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_color(bar, COLOR_DIM, 0);
  lv_obj_set_style_pad_all(bar, 2, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  listTitle = lv_label_create(bar);
  lv_label_set_text(listTitle, "// VAULT :: ---");
  lv_obj_set_style_text_color(listTitle, COLOR_FG, 0);
  lv_obj_align(listTitle, LV_ALIGN_LEFT_MID, 4, 0);

  // Tamper indicator (small, dim)
  tamperLabel = lv_label_create(bar);
  lv_label_set_text(tamperLabel, "");
  lv_obj_set_style_text_color(tamperLabel, COLOR_DIM, 0);
  lv_obj_align(tamperLabel, LV_ALIGN_LEFT_MID, 200, 0);

  // BLE dot
  bleDot = lv_obj_create(bar);
  lv_obj_set_size(bleDot, 6, 6);
  lv_obj_set_style_bg_color(bleDot, COLOR_GREEN, 0);
  lv_obj_set_style_border_width(bleDot, 0, 0);
  lv_obj_set_style_radius(bleDot, 0, 0);
  lv_obj_clear_flag(bleDot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(bleDot, LV_ALIGN_RIGHT_MID, -200, 0);

  battLabel = lv_label_create(bar);
  lv_label_set_text(battLabel, "100%");
  lv_obj_set_style_text_color(battLabel, COLOR_GREEN, 0);
  lv_obj_align(battLabel, LV_ALIGN_RIGHT_MID, -160, 0);

  // Buttons: + / SRCH / CFG / LOCK
  struct { const char* label; lv_color_t color; int xRight; void(*cb)(lv_event_t*); } btns[] = {
    {"+",    COLOR_GREEN,   -152, [](lv_event_t*){
      if (!App::currentVault) return;
      Link newLink;
      newLink.name = "new link";
      newLink.cat  = (App::activeCategory != "ALL") ? App::activeCategory : "";
      App::currentVault->links.push_back(newLink);
      UIEdit::editLink(&App::currentVault->links.back(), [](){
        renderTabStrip();
        renderRows();
      }, true);
    }},
    {"SRCH", COLOR_FG,     -110, [](lv_event_t*){ openSearch(); }},
    {"CFG",  COLOR_FG,      -68, [](lv_event_t*){ uiConfig_show(); }},
    {"LOCK", COLOR_ACCENT,   -2, [](lv_event_t*){ App::lockNow(); }},
  };
  for (int i = 0; i < 4; i++) {
    lv_obj_t* b = lv_btn_create(bar);
    lv_obj_set_size(b, btns[i].label[0]=='L' ? 60 : 40, 18);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, btns[i].xRight, 0);
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

  // ----- TAB STRIP -----
  tabStrip = lv_obj_create(scrList);
  lv_obj_set_size(tabStrip, SCREEN_W, 20);
  lv_obj_set_pos(tabStrip, 0, 22);
  lv_obj_set_style_bg_color(tabStrip, COLOR_BG, 0);
  lv_obj_set_style_border_side(tabStrip, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(tabStrip, 1, 0);
  lv_obj_set_style_border_color(tabStrip, COLOR_DIM, 0);
  lv_obj_set_style_pad_all(tabStrip, 2, 0);
  lv_obj_set_style_pad_gap(tabStrip, 3, 0);
  lv_obj_set_flex_flow(tabStrip, LV_FLEX_FLOW_ROW);
  lv_obj_set_scroll_dir(tabStrip, LV_DIR_HOR);

  // ----- ROWS CONTAINER -----
  rowsContainer = lv_obj_create(scrList);
  lv_obj_set_size(rowsContainer, SCREEN_W, SCREEN_H - 42);
  lv_obj_set_pos(rowsContainer, 0, 42);
  lv_obj_set_style_bg_color(rowsContainer, COLOR_BG, 0);
  lv_obj_set_style_border_width(rowsContainer, 0, 0);
  lv_obj_set_style_pad_all(rowsContainer, 3, 0);
  lv_obj_set_style_pad_gap(rowsContainer, 2, 0);
  lv_obj_set_flex_flow(rowsContainer, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(rowsContainer, LV_DIR_VER);

  UI::addScanline(scrList);
  buildSearchOverlay();
}

// =========================================================================
//  RENDER TOP BAR (called per show — vault name etc.)
// =========================================================================
static void renderTopbar() {
  if (!App::currentVault) return;
  String t = "// VAULT :: " + App::currentVault->name;
  t.toUpperCase();
  lv_label_set_text(listTitle, t.c_str());

  // BLE status
  bool conn = bleKeyboard.isConnected();
  lv_obj_set_style_bg_color(bleDot, conn ? COLOR_GREEN : COLOR_RED, 0);

  int bp = batteryRead();
  String bs = String(bp) + "%";
  lv_label_set_text(battLabel, bs.c_str());
  lv_obj_set_style_text_color(battLabel, bp < 20 ? COLOR_ACCENT : COLOR_GREEN, 0);
}

// =========================================================================
//  TAB STRIP
// =========================================================================
static void renderTabStrip() {
  lv_obj_clean(tabStrip);
  if (!App::currentVault) return;

  auto makeTab = [](const char* text, bool active, lv_color_t color, lv_event_cb_t click, void* userData, std::function<void()> longPress) -> lv_obj_t* {
    lv_obj_t* t = lv_btn_create(tabStrip);
    lv_obj_set_height(t, 14);
    lv_obj_set_style_pad_hor(t, 6, 0);
    lv_obj_set_style_pad_ver(t, 0, 0);
    lv_obj_set_style_bg_color(t, active ? color : COLOR_BG, 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_border_color(t, active ? color : COLOR_DIM, 0);
    lv_obj_set_style_radius(t, 0, 0);
    lv_obj_set_style_shadow_width(t, 0, 0);
    lv_obj_t* l = lv_label_create(t);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, active ? COLOR_BG : color, 0);
    lv_obj_center(l);
    lv_obj_set_user_data(t, userData);
    lv_obj_add_event_cb(t, click, LV_EVENT_CLICKED, userData);
    if (longPress) UI::bindLongPress(t, 600, longPress);
    return t;
  };

  // ALL tab
  bool allActive = (App::activeCategory == "ALL");
  makeTab("all", allActive, COLOR_FG, [](lv_event_t*){
    App::activeCategory = "ALL";
    renderTabStrip();
    renderRows();
  }, NULL, nullptr);

  // Per-category tabs
  for (auto& c : App::currentVault->categories) {
    bool active = (App::activeCategory == c.name);
    String* nameP = new String(c.name);
    Category* catP = &c;
    lv_obj_t* tab = makeTab(c.name.c_str(), active, lv_color_hex(c.color), [](lv_event_t* e){
      String* nm = (String*)lv_obj_get_user_data(lv_event_get_target(e));
      App::activeCategory = *nm;
      renderTabStrip();
      renderRows();
    }, nameP, [catP](){
      UIEdit::editCategory(catP, false, [](){
        renderTabStrip();
        renderRows();
      });
    });
    // Free the heap String when the tab is deleted (on lv_obj_clean or re-render).
    lv_obj_add_event_cb(tab, [](lv_event_t* e){
      delete (String*)lv_obj_get_user_data(lv_event_get_target(e));
    }, LV_EVENT_DELETE, NULL);
  }

  // "+" tab (add new category)
  lv_obj_t* addBtn = lv_btn_create(tabStrip);
  lv_obj_set_height(addBtn, 14);
  lv_obj_set_style_pad_hor(addBtn, 8, 0);
  lv_obj_set_style_pad_ver(addBtn, 0, 0);
  lv_obj_set_style_bg_color(addBtn, COLOR_BG, 0);
  lv_obj_set_style_border_width(addBtn, 1, 0);
  lv_obj_set_style_border_color(addBtn, COLOR_DIM, 0);
  lv_obj_set_style_radius(addBtn, 0, 0);
  lv_obj_set_style_shadow_width(addBtn, 0, 0);
  lv_obj_t* al = lv_label_create(addBtn);
  lv_label_set_text(al, "+");
  lv_obj_set_style_text_color(al, COLOR_ACCENT, 0);
  lv_obj_center(al);
  lv_obj_add_event_cb(addBtn, [](lv_event_t*){
    UIEdit::editCategory(nullptr, true, [](){
      renderTabStrip();
      renderRows();
    });
  }, LV_EVENT_CLICKED, NULL);
}

// =========================================================================
//  ROWS
// =========================================================================
static void renderRows() {
  lv_obj_clean(rowsContainer);
  if (!App::currentVault) return;

  std::vector<Link*> filtered;
  for (auto& l : App::currentVault->links) {
    if (App::activeCategory == "ALL" || App::activeCategory == l.cat) filtered.push_back(&l);
  }

  if (filtered.empty()) {
    lv_obj_t* e = lv_label_create(rowsContainer);
    lv_label_set_text(e, "[ no links in this category :: tap CFG to add ]");
    lv_obj_set_style_text_color(e, COLOR_DIM, 0);
    return;
  }

  int idx = 0;
  for (Link* l : filtered) {
    Link* lp = l;
    int rowIdx = idx++;
    lv_obj_t* row = lv_btn_create(rowsContainer);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 22);
    lv_obj_set_style_bg_color(row, COLOR_ROW, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, COLOR_DIM, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_hor(row, 6, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    char prefix[8];
    snprintf(prefix, sizeof(prefix), "[%02d]", rowIdx + 1);
    lv_obj_t* idxL = lv_label_create(row);
    lv_label_set_text(idxL, prefix);
    lv_obj_set_style_text_color(idxL, COLOR_DIM, 0);
    lv_obj_set_style_pad_right(idxL, 4, 0);

    // Category pill
    if (lp->cat.length() > 0) {
      // Find color
      uint32_t color = 0xFFB000;
      for (auto& c : App::currentVault->categories) {
        if (c.name == lp->cat) { color = c.color; break; }
      }
      lv_obj_t* pill = lv_btn_create(row);
      lv_obj_set_height(pill, 14);
      lv_obj_set_style_pad_hor(pill, 4, 0);
      lv_obj_set_style_pad_ver(pill, 0, 0);
      lv_obj_set_style_bg_color(pill, COLOR_BG, 0);
      lv_obj_set_style_border_width(pill, 1, 0);
      lv_obj_set_style_border_color(pill, lv_color_hex(color), 0);
      lv_obj_set_style_radius(pill, 0, 0);
      lv_obj_set_style_shadow_width(pill, 0, 0);
      lv_obj_set_style_margin_right(pill, 4, 0);
      lv_obj_t* pl = lv_label_create(pill);
      lv_label_set_text(pl, lp->cat.c_str());
      lv_obj_set_style_text_color(pl, lv_color_hex(color), 0);
      lv_obj_set_style_text_font(pl, FONT_SMALL, 0);
      lv_obj_center(pl);
      String* catName = new String(lp->cat);
      lv_obj_set_user_data(pill, catName);
      lv_obj_add_event_cb(pill, [](lv_event_t* e){
        lv_obj_t* target = lv_event_get_target(e);
        String* n = (String*)lv_obj_get_user_data(target);
        if (lv_event_get_code(e) == LV_EVENT_DELETE) { delete n; return; }
        App::activeCategory = *n;
        renderTabStrip();
        renderRows();
      }, LV_EVENT_ALL, NULL);
    }

    lv_obj_t* nameL = lv_label_create(row);
    lv_label_set_text(nameL, lp->name.c_str());
    lv_obj_set_style_text_color(nameL, COLOR_FG, 0);
    lv_obj_set_flex_grow(nameL, 1);

    lv_obj_t* arrow = lv_label_create(row);
    lv_label_set_text(arrow, ">>");
    lv_obj_set_style_text_color(arrow, COLOR_DIM, 0);

    // Tap row → transmit
    lv_obj_set_user_data(row, lp);
    lv_obj_add_event_cb(row, [](lv_event_t* e){
      Link* l = (Link*)lv_obj_get_user_data(lv_event_get_target(e));
      if (!l) return;
      App::notifyActivity();
      uiTransmit_url(l->url);
    }, LV_EVENT_CLICKED, NULL);

    UI::bindLongPress(row, 600, [lp](){
      App::notifyActivity();
      UIEdit::editLink(lp, [](){
        renderTabStrip();
        renderRows();
      });
    });
  }
}

// =========================================================================
//  TRANSMIT URL OVER BLE
// =========================================================================
void uiTransmit_url(const String& url) {
  if (!bleKeyboard.isConnected()) {
    UI::toast("!! BLE OFFLINE !!", COLOR_ACCENT);
    return;
  }
  // Show overlay
  lv_obj_t* ov = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ov, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(ov, 0, 0);
  lv_obj_set_style_bg_color(ov, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_90, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* lab = lv_label_create(ov);
  lv_obj_set_style_text_font(lab, FONT_LARGE, 0);
  lv_obj_set_style_text_color(lab, COLOR_GREEN, 0);
  lv_label_set_text(lab, ">> TRANSMITTING <<");
  lv_obj_align(lab, LV_ALIGN_CENTER, 0, -12);
  UI::glitchLabel(lab, ">> TRANSMITTING <<", 800);

  lv_obj_t* sub = lv_label_create(ov);
  lv_obj_set_style_text_color(sub, COLOR_DIM, 0);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sub, SCREEN_W - 40);
  lv_label_set_text(sub, url.c_str());
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 20);

  delay(250);
  bleKeyboard.print(url.c_str());
  if (App::currentVault && App::currentVault->settings.autoEnter) {
    bleKeyboard.write(KEY_RETURN);
  }

  // Auto-dismiss
  lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
    lv_obj_del((lv_obj_t*)tm->user_data);
    lv_timer_del(tm);
  }, 1400, ov);
  lv_timer_set_repeat_count(t, 1);
}

// =========================================================================
//  TRANSMIT CREDENTIALS (username Tab password) OVER BLE
// =========================================================================
void uiTransmit_creds(const String& username, const String& password) {
  if (!bleKeyboard.isConnected()) {
    UI::toast("!! BLE OFFLINE !!", COLOR_ACCENT);
    return;
  }
  if (username.length() == 0 && password.length() == 0) {
    UI::toast("no credentials stored", COLOR_ACCENT);
    return;
  }

  lv_obj_t* ov = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ov, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(ov, 0, 0);
  lv_obj_set_style_bg_color(ov, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_90, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* lab = lv_label_create(ov);
  lv_obj_set_style_text_font(lab, FONT_LARGE, 0);
  lv_obj_set_style_text_color(lab, COLOR_GREEN, 0);
  lv_label_set_text(lab, ">> LOGIN <<");
  lv_obj_align(lab, LV_ALIGN_CENTER, 0, -12);
  UI::glitchLabel(lab, ">> LOGIN <<", 800);

  lv_obj_t* sub = lv_label_create(ov);
  lv_obj_set_style_text_color(sub, COLOR_DIM, 0);
  lv_label_set_text(sub, username.length() ? username.c_str() : "(no username)");
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 20);

  delay(250);
  if (username.length() > 0) bleKeyboard.print(username.c_str());
  if (username.length() > 0 && password.length() > 0) bleKeyboard.write(KEY_TAB);
  if (password.length() > 0) bleKeyboard.print(password.c_str());
  if (App::currentVault && App::currentVault->settings.autoEnter) {
    bleKeyboard.write(KEY_RETURN);
  }

  lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
    lv_obj_del((lv_obj_t*)tm->user_data);
    lv_timer_del(tm);
  }, 1400, ov);
  lv_timer_set_repeat_count(t, 1);
}

// =========================================================================
//  SEARCH OVERLAY
// =========================================================================
static void buildSearchOverlay() {
  searchOverlay = lv_obj_create(scrList);
  lv_obj_set_size(searchOverlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(searchOverlay, 0, 0);
  lv_obj_set_style_bg_color(searchOverlay, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(searchOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(searchOverlay, 0, 0);
  lv_obj_set_style_pad_all(searchOverlay, 4, 0);
  lv_obj_clear_flag(searchOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(searchOverlay, LV_OBJ_FLAG_HIDDEN);

  // Title
  lv_obj_t* title = lv_label_create(searchOverlay);
  lv_label_set_text(title, "// SEARCH");
  lv_obj_set_style_text_color(title, COLOR_FG, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* done = lv_btn_create(searchOverlay);
  lv_obj_set_size(done, 60, 16);
  lv_obj_align(done, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(done, COLOR_BG, 0);
  lv_obj_set_style_border_width(done, 1, 0);
  lv_obj_set_style_border_color(done, COLOR_FG, 0);
  lv_obj_set_style_radius(done, 0, 0);
  lv_obj_set_style_shadow_width(done, 0, 0);
  lv_obj_t* dl = lv_label_create(done);
  lv_label_set_text(dl, "DONE");
  lv_obj_set_style_text_color(dl, COLOR_FG, 0);
  lv_obj_center(dl);
  lv_obj_add_event_cb(done, [](lv_event_t*){ closeSearch(); }, LV_EVENT_CLICKED, NULL);

  // Input field
  searchInput = lv_label_create(searchOverlay);
  lv_obj_set_style_bg_color(searchInput, COLOR_ROW, 0);
  lv_obj_set_style_bg_opa(searchInput, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(searchInput, 1, 0);
  lv_obj_set_style_border_color(searchInput, COLOR_DIM, 0);
  lv_obj_set_style_pad_hor(searchInput, 4, 0);
  lv_obj_set_style_pad_ver(searchInput, 1, 0);
  lv_obj_set_style_text_color(searchInput, COLOR_FG, 0);
  lv_obj_set_size(searchInput, 240, 16);
  lv_obj_align(searchInput, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_label_set_text(searchInput, "_");

  // Results
  searchResults = lv_obj_create(searchOverlay);
  lv_obj_set_size(searchResults, 240, SCREEN_H - 40);
  lv_obj_align(searchResults, LV_ALIGN_TOP_LEFT, 0, 38);
  lv_obj_set_style_bg_opa(searchResults, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(searchResults, 0, 0);
  lv_obj_set_style_pad_all(searchResults, 0, 0);
  lv_obj_set_style_pad_gap(searchResults, 1, 0);
  lv_obj_set_flex_flow(searchResults, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(searchResults, LV_DIR_VER);

  // QWERTY on the right
  lv_obj_t* kb = lv_obj_create(searchOverlay);
  lv_obj_set_size(kb, SCREEN_W - 248, SCREEN_H - 8);
  lv_obj_set_pos(kb, 244, 4);
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(kb, 0, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);

  UI::QwertyCallbacks cbs;
  cbs.onChar = [](char c){
    App::notifyActivity();
    searchBuffer += c;
    renderSearchResults();
  };
  cbs.onDelete = [](){
    App::notifyActivity();
    if (searchBuffer.length() > 0) {
      searchBuffer.remove(searchBuffer.length() - 1);
      renderSearchResults();
    }
  };
  cbs.onOk = [](){
    closeSearch();
  };
  UI::buildQwerty(kb, cbs);
}

static void openSearch() {
  searchBuffer = "";
  lv_label_set_text(searchInput, "_");
  lv_obj_clean(searchResults);
  lv_obj_clear_flag(searchOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void closeSearch() {
  lv_obj_add_flag(searchOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void renderSearchResults() {
  String show = searchBuffer + "_";
  lv_label_set_text(searchInput, show.c_str());

  lv_obj_clean(searchResults);
  if (!App::currentVault || searchBuffer.length() == 0) return;

  String q = searchBuffer; q.toLowerCase();

  for (auto& l : App::currentVault->links) {
    String n = l.name; n.toLowerCase();
    String u = l.url;  u.toLowerCase();
    if (n.indexOf(q) < 0 && u.indexOf(q) < 0) continue;

    Link* lp = &l;
    lv_obj_t* row = lv_btn_create(searchResults);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 18);
    lv_obj_set_style_bg_color(row, COLOR_ROW, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, COLOR_DIM, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_hor(row, 4, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);

    lv_obj_t* nameL = lv_label_create(row);
    lv_label_set_text(nameL, lp->name.c_str());
    lv_obj_set_style_text_color(nameL, COLOR_FG, 0);
    lv_obj_align(nameL, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_set_user_data(row, lp);
    lv_obj_add_event_cb(row, [](lv_event_t* e){
      Link* l = (Link*)lv_obj_get_user_data(lv_event_get_target(e));
      closeSearch();
      uiTransmit_url(l->url);
    }, LV_EVENT_CLICKED, NULL);
  }
}

// =========================================================================
//  BATTERY READ
// =========================================================================
// Stub. Replace with the actual ADC read from Waveshare's battery demo.
// Channel 3 on this board reads battery voltage through their divider.
int batteryRead() {
  // TODO: Replace with Waveshare's reference impl. See README for guidance.
  return 87;
}
