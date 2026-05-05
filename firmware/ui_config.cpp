/*
 * ui_config.cpp - Settings screen
 *
 * Visible by default: this vault, backup
 * Hidden until long-press BACK button: admin section (master password gated)
 *
 * Decoy identity badge: shown for 5sec when long-pressing config title (only if
 * current vault is the decoy; real vaults give no feedback).
 */
#include <lvgl.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <BleKeyboard.h>
#include "config.h"
#include "theme.h"
#include "app_state.h"
#include "ui_common.h"
#include "ui_qwerty.h"
#include "ui_edit.h"
#include "vaults.h"
#include "storage.h"
#include "ble_proximity.h"

extern void uiList_show();
extern BleKeyboard bleKeyboard;   // defined in LinkVault.ino

// =========================================================================
//  STATE
// =========================================================================
static lv_obj_t* scrConfig;
static lv_obj_t* configTitle;
static lv_obj_t* vaultBadge;     // [DECOY] badge - hidden by default
static lv_obj_t* configBody;
static lv_obj_t* backBtn;
static bool adminRevealed = false;
static AsyncWebServer* configServer = nullptr;
static bool configWifiActive = false;

// Forward
static void renderConfig();
static void requireAdmin(std::function<void()> cb);
static void startConfigWifi();
static void stopConfigWifi();

// =========================================================================
//  ENTRY
// =========================================================================
void uiConfig_build() {
  scrConfig = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scrConfig, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scrConfig, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scrConfig, 0, 0);
  lv_obj_clear_flag(scrConfig, LV_OBJ_FLAG_SCROLLABLE);

  // Topbar
  lv_obj_t* bar = lv_obj_create(scrConfig);
  lv_obj_set_size(bar, SCREEN_W, 22);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, COLOR_BG, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_color(bar, COLOR_DIM, 0);
  lv_obj_set_style_pad_all(bar, 2, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  configTitle = lv_label_create(bar);
  lv_label_set_text(configTitle, "// CONFIG :: ---");
  lv_obj_set_style_text_color(configTitle, COLOR_ACCENT, 0);
  lv_obj_align(configTitle, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_add_flag(configTitle, LV_OBJ_FLAG_CLICKABLE);

  // Decoy badge (hidden by default; flashed on long-press if decoy)
  vaultBadge = lv_label_create(bar);
  lv_label_set_text(vaultBadge, "[ DECOY ]");
  lv_obj_set_style_text_color(vaultBadge, COLOR_RED, 0);
  lv_obj_set_style_border_width(vaultBadge, 1, 0);
  lv_obj_set_style_border_color(vaultBadge, COLOR_RED, 0);
  lv_obj_set_style_pad_hor(vaultBadge, 4, 0);
  lv_obj_align(vaultBadge, LV_ALIGN_LEFT_MID, 200, 0);
  lv_obj_add_flag(vaultBadge, LV_OBJ_FLAG_HIDDEN);

  // BACK button (long-press = reveal admin)
  backBtn = lv_btn_create(bar);
  lv_obj_set_size(backBtn, 60, 18);
  lv_obj_align(backBtn, LV_ALIGN_RIGHT_MID, -2, 0);
  lv_obj_set_style_bg_color(backBtn, COLOR_BG, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COLOR_FG, 0);
  lv_obj_set_style_radius(backBtn, 0, 0);
  lv_obj_set_style_shadow_width(backBtn, 0, 0);
  lv_obj_t* bl = lv_label_create(backBtn);
  lv_label_set_text(bl, "BACK");
  lv_obj_set_style_text_color(bl, COLOR_FG, 0);
  lv_obj_center(bl);
  lv_obj_add_event_cb(backBtn, [](lv_event_t*){
    adminRevealed = false;
    if (configWifiActive) stopConfigWifi();
    uiList_show();
  }, LV_EVENT_CLICKED, NULL);

  // Body (scrollable)
  configBody = lv_obj_create(scrConfig);
  lv_obj_set_size(configBody, SCREEN_W, SCREEN_H - 22);
  lv_obj_set_pos(configBody, 0, 22);
  lv_obj_set_style_bg_color(configBody, COLOR_BG, 0);
  lv_obj_set_style_border_width(configBody, 0, 0);
  lv_obj_set_style_pad_all(configBody, 4, 0);
  lv_obj_set_style_pad_gap(configBody, 1, 0);
  lv_obj_set_flex_flow(configBody, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(configBody, LV_DIR_VER);

  UI::addScanline(scrConfig);

  // Long-press handlers (bind once, behavior depends on current state)
  UI::bindLongPress(configTitle, 700, [](){
    if (App::currentVault && App::currentVault->isDecoy) {
      // Briefly show the [DECOY] badge
      lv_obj_clear_flag(vaultBadge, LV_OBJ_FLAG_HIDDEN);
      lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
        lv_obj_add_flag(vaultBadge, LV_OBJ_FLAG_HIDDEN);
        lv_timer_del(tm);
      }, DECOY_BADGE_MS, nullptr);
      lv_timer_set_repeat_count(t, 1);
    }
    // Real vault: no feedback (deniable)
  });
  UI::bindLongPress(backBtn, 700, [](){
    if (!adminRevealed) {
      adminRevealed = true;
      renderConfig();
    }
  });
}

void uiConfig_show() {
  if (!App::currentVault) return;
  adminRevealed = false;   // always hidden on entry
  App::state = STATE_CONFIG;
  App::lastActivityMs = millis();
  renderConfig();
  UI::wipeTransition(scrConfig);
}

// =========================================================================
//  RENDER
// =========================================================================
static void addSection(const char* text) {
  lv_obj_t* d = lv_label_create(configBody);
  lv_label_set_text(d, text);
  lv_obj_set_style_text_color(d, COLOR_DIM, 0);
  lv_obj_set_style_pad_top(d, 4, 0);
  lv_obj_set_style_pad_bottom(d, 1, 0);
}

static void addItem(const char* label, const char* val, lv_color_t valColor, std::function<void()> onClick) {
  lv_obj_t* d = lv_btn_create(configBody);
  lv_obj_set_width(d, lv_pct(100));
  lv_obj_set_height(d, 16);
  lv_obj_set_style_bg_color(d, COLOR_BG, 0);
  lv_obj_set_style_border_width(d, 0, 0);
  lv_obj_set_style_radius(d, 0, 0);
  lv_obj_set_style_shadow_width(d, 0, 0);
  lv_obj_set_style_pad_hor(d, 4, 0);
  lv_obj_set_style_pad_ver(d, 0, 0);

  lv_obj_t* lab = lv_label_create(d);
  lv_label_set_text(lab, label);
  lv_obj_set_style_text_color(lab, COLOR_FG, 0);
  lv_obj_align(lab, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t* v = lv_label_create(d);
  lv_label_set_text(v, val);
  lv_obj_set_style_text_color(v, valColor, 0);
  lv_obj_align(v, LV_ALIGN_RIGHT_MID, 0, 0);

  auto* cb = new std::function<void()>(onClick);
  lv_obj_set_user_data(d, cb);
  lv_obj_add_event_cb(d, [](lv_event_t* e){
    lv_obj_t* target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    auto* fn = (std::function<void()>*)lv_obj_get_user_data(target);
    if (code == LV_EVENT_DELETE) { delete fn; return; }
    if (code == LV_EVENT_CLICKED && fn && *fn) (*fn)();
  }, LV_EVENT_ALL, NULL);
}

static void renderConfig() {
  lv_obj_clean(configBody);
  Vault* v = App::currentVault;
  if (!v) return;

  String title = "// CONFIG :: " + v->name;
  lv_label_set_text(configTitle, title.c_str());
  lv_obj_add_flag(vaultBadge, LV_OBJ_FLAG_HIDDEN);

  // ----- THIS VAULT -----
  String secLabel = "this vault [" + v->name + "]";
  addSection(secLabel.c_str());

  addItem("add new link", "+", COLOR_GREEN, [v](){
    Link nw;
    nw.name = "new link";
    nw.url = "https://";
    nw.cat = v->categories.empty() ? "" : v->categories[0].name;
    nw.lastUsed = 0;
    nw.timesUsed = 0;
    v->links.push_back(nw);
    Vaults::persist();
    Link* lp = &v->links.back();
    uiList_show();
    lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
      Link* l = (Link*)tm->user_data;
      UIEdit::editLink(l, [](){ uiList_show(); });
      lv_timer_del(tm);
    }, 250, lp);
    lv_timer_set_repeat_count(t, 1);
  });

  String catVal = String(v->categories.size()) + " :: long-press tab to edit";
  addItem("manage categories", catVal.c_str(), COLOR_DIM, [](){
    UIEdit::editCategory(nullptr, true, [](){ renderConfig(); });
  });

  addItem("auto-press enter", v->settings.autoEnter ? "ON" : "OFF",
    v->settings.autoEnter ? COLOR_GREEN : COLOR_DIM, [v](){
      v->settings.autoEnter = !v->settings.autoEnter;
      Vaults::persist();
      renderConfig();
    });

  String slpVal = String(v->settings.sleepMinutes) + " min";
  addItem("deep sleep timeout", slpVal.c_str(), COLOR_GREEN, [v](){
    int idx = 0;
    for (int i = 0; i < SLEEP_CYCLE_COUNT; i++) {
      if (SLEEP_CYCLE_MIN[i] == v->settings.sleepMinutes) { idx = i; break; }
    }
    idx = (idx + 1) % SLEEP_CYCLE_COUNT;
    v->settings.sleepMinutes = SLEEP_CYCLE_MIN[idx];
    Vaults::persist();
    renderConfig();
  });

  // Change PIN for this vault — uses confirm dialog + new pin entry?
  // Simpler: a sub-screen would be more correct, but for now, we redirect
  // through the recovery-style new-pin entry. Skipped here for brevity in
  // this build; can be added later as a small dedicated screen.

  // ----- BACKUP -----
  addSection("backup / wifi config");
  addItem("start wifi config",
    configWifiActive ? "RUNNING" : "OFF",
    configWifiActive ? COLOR_GREEN : COLOR_DIM,
    [](){
      if (configWifiActive) stopConfigWifi();
      else startConfigWifi();
      renderConfig();
    });
  if (configWifiActive) {
    addItem("ssid", AP_SSID, COLOR_FG, [](){});
    addItem("password", AP_PASSWORD, COLOR_FG, [](){});
    addItem("url", "http://192.168.4.1", COLOR_FG, [](){});
  }

  // ----- ADMIN (hidden by default) -----
  if (adminRevealed) {
    addSection("admin (master password)");
    addItem("pair new phone", "+", COLOR_GREEN, [](){
      requireAdmin([](){
        bleProximity_startPairing();
        // Show the pair code overlay
        lv_obj_t* ov = lv_obj_create(lv_layer_top());
        lv_obj_set_size(ov, SCREEN_W, SCREEN_H);
        lv_obj_set_pos(ov, 0, 0);
        lv_obj_set_style_bg_color(ov, COLOR_BG, 0);
        lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ov, 1, 0);
        lv_obj_set_style_border_color(ov, COLOR_FG, 0);
        lv_obj_set_style_pad_all(ov, 8, 0);
        lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* t = lv_label_create(ov);
        lv_label_set_text(t, "// PAIRING MODE");
        lv_obj_set_style_text_color(t, COLOR_ACCENT, 0);
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t* code = lv_label_create(ov);
        char buf[12];
        snprintf(buf, sizeof(buf), "%06u", bleProximity_currentPairCode());
        lv_label_set_text(code, buf);
        lv_obj_set_style_text_color(code, COLOR_FG, 0);
        lv_obj_set_style_text_font(code, FONT_LARGE, 0);
        lv_obj_align(code, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t* hint = lv_label_create(ov);
        lv_label_set_text(hint, "On phone: Settings > Bluetooth > pair 'Link Vault' and confirm matching code");
        lv_obj_set_style_text_color(hint, COLOR_DIM, 0);
        lv_obj_set_style_text_font(hint, FONT_SMALL, 0);
        lv_obj_set_width(hint, SCREEN_W - 16);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 0, -20);

        lv_obj_t* close = lv_btn_create(ov);
        lv_obj_set_size(close, 80, 20);
        lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(close, COLOR_BG, 0);
        lv_obj_set_style_border_width(close, 1, 0);
        lv_obj_set_style_border_color(close, COLOR_FG, 0);
        lv_obj_set_style_radius(close, 0, 0);
        lv_obj_set_style_shadow_width(close, 0, 0);
        lv_obj_t* cl = lv_label_create(close);
        lv_label_set_text(cl, "DONE");
        lv_obj_set_style_text_color(cl, COLOR_FG, 0);
        lv_obj_center(cl);
        lv_obj_set_user_data(close, ov);
        lv_obj_add_event_cb(close, [](lv_event_t* e){
          bleProximity_cancelPairing();
          lv_obj_t* o = (lv_obj_t*)lv_obj_get_user_data(lv_event_get_target(e));
          lv_obj_del(o);
        }, LV_EVENT_CLICKED, NULL);
      });
    });

    int bondCount = (int)bleProximity_bondedList().size();
    addItem("bonded phones", String(bondCount).c_str(),
      bondCount > 0 ? COLOR_GREEN : COLOR_DIM, [](){
        requireAdmin([](){
          auto bonded = bleProximity_bondedList();
          String list = "";
          for (auto& a : bonded) list += a + "\n";
          if (list.length() == 0) list = "(none)";
          UI::confirmDialog("// BONDED DEVICES", list.c_str(), "FORGET ALL", true,
            [bonded](bool ok){
              if (!ok) return;
              for (auto& a : bonded) bleProximity_unbond(a);
              UI::toast("all bonds cleared", COLOR_ACCENT);
              renderConfig();
            });
        });
      });

    GlobalSettings g = Storage::loadGlobalSettings();
    addItem("proximity override (master pw)", g.proximityOverrideEnabled ? "ON" : "OFF",
      g.proximityOverrideEnabled ? COLOR_GREEN : COLOR_DIM, [](){
        requireAdmin([](){
          GlobalSettings g = Storage::loadGlobalSettings();
          g.proximityOverrideEnabled = !g.proximityOverrideEnabled;
          Storage::saveGlobalSettings(g);
          renderConfig();
        });
      });

    addItem("view all vaults", "→", COLOR_ACCENT, [](){
      requireAdmin([](){
        String s = "";
        for (auto& v : Vaults::all()) {
          s += v.name + (v.isDecoy ? " [DECOY]" : "") + "\n";
        }
        UI::confirmDialog("// ALL VAULTS", s.c_str(), "OK", false, [](bool){});
      });
    });

    addItem("create new vault", "+", COLOR_ACCENT, [](){
      requireAdmin([](){
        if (Vaults::all().size() >= MAX_VAULTS) {
          UI::toast("vault limit reached", COLOR_ACCENT);
          return;
        }
        UI::toast("use admin tool / config page", COLOR_DIM);
        // Full create-vault UI would be its own dedicated flow.
      });
    });

    addItem("change master password", "→", COLOR_ACCENT, [](){
      requireAdmin([](){
        UI::toast("use wifi config page", COLOR_DIM);
      });
    });

    addItem("factory reset", "WIPE", COLOR_ACCENT, [](){
      requireAdmin([](){
        UI::confirmDialog("// FACTORY RESET",
          "Wipe ALL vaults, links, settings, and bonds. Cannot be undone.",
          "WIPE", true, [](bool ok){
            if (!ok) return;
            Storage::factoryReset();
            UI::toast("wiped — rebooting", COLOR_ACCENT);
            delay(800);
            ESP.restart();
          });
      });
    });

    addItem("hide admin section", "X", COLOR_DIM, [](){
      adminRevealed = false;
      renderConfig();
    });
  }
}

// =========================================================================
//  ADMIN AUTH GATE
// =========================================================================
struct AdminCtx {
  String buf;
  std::function<void()> cb;
  lv_obj_t* overlay;
  lv_obj_t* display;
};
static AdminCtx* g_adminCtx = nullptr;

static void closeAdmin(bool runCb) {
  if (!g_adminCtx) return;
  auto cb = g_adminCtx->cb;
  lv_obj_del(g_adminCtx->overlay);
  delete g_adminCtx;
  g_adminCtx = nullptr;
  if (runCb && cb) cb();
}

static void requireAdmin(std::function<void()> cb) {
  if (g_adminCtx) closeAdmin(false);
  g_adminCtx = new AdminCtx();
  g_adminCtx->cb = cb;

  g_adminCtx->overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g_adminCtx->overlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(g_adminCtx->overlay, 0, 0);
  lv_obj_set_style_bg_color(g_adminCtx->overlay, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(g_adminCtx->overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_adminCtx->overlay, 1, 0);
  lv_obj_set_style_border_color(g_adminCtx->overlay, COLOR_ACCENT, 0);
  lv_obj_set_style_pad_all(g_adminCtx->overlay, 4, 0);
  lv_obj_clear_flag(g_adminCtx->overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(g_adminCtx->overlay);
  lv_label_set_text(title, "// ADMIN AUTH REQUIRED");
  lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t* close = lv_btn_create(g_adminCtx->overlay);
  lv_obj_set_size(close, 70, 16);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(close, COLOR_BG, 0);
  lv_obj_set_style_border_width(close, 1, 0);
  lv_obj_set_style_border_color(close, COLOR_FG, 0);
  lv_obj_set_style_radius(close, 0, 0);
  lv_obj_set_style_shadow_width(close, 0, 0);
  lv_obj_t* cl = lv_label_create(close);
  lv_label_set_text(cl, "X CLOSE");
  lv_obj_set_style_text_color(cl, COLOR_FG, 0);
  lv_obj_center(cl);
  lv_obj_add_event_cb(close, [](lv_event_t*){ closeAdmin(false); }, LV_EVENT_CLICKED, NULL);

  g_adminCtx->display = lv_label_create(g_adminCtx->overlay);
  lv_obj_set_style_bg_color(g_adminCtx->display, COLOR_ROW, 0);
  lv_obj_set_style_bg_opa(g_adminCtx->display, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_adminCtx->display, 1, 0);
  lv_obj_set_style_border_color(g_adminCtx->display, COLOR_ACCENT, 0);
  lv_obj_set_style_pad_hor(g_adminCtx->display, 4, 0);
  lv_obj_set_style_text_color(g_adminCtx->display, COLOR_FG, 0);
  lv_obj_set_size(g_adminCtx->display, SCREEN_W - 8, 16);
  lv_obj_align(g_adminCtx->display, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_label_set_text(g_adminCtx->display, "_");

  lv_obj_t* kb = lv_obj_create(g_adminCtx->overlay);
  lv_obj_set_size(kb, SCREEN_W - 8, SCREEN_H - 44);
  lv_obj_set_pos(kb, 0, 38);
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(kb, 0, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);

  UI::QwertyCallbacks cbs;
  cbs.onChar = [](char c){
    if (g_adminCtx->buf.length() < 64) {
      g_adminCtx->buf += c;
      String s = "";
      for (size_t i = 0; i < g_adminCtx->buf.length(); i++) s += "*";
      s += "_";
      lv_label_set_text(g_adminCtx->display, s.c_str());
    }
  };
  cbs.onDelete = [](){
    if (g_adminCtx->buf.length() > 0) {
      g_adminCtx->buf.remove(g_adminCtx->buf.length() - 1);
      String s = "";
      for (size_t i = 0; i < g_adminCtx->buf.length(); i++) s += "*";
      s += "_";
      lv_label_set_text(g_adminCtx->display, s.c_str());
    }
  };
  cbs.onOk = [](){
    String stored = Storage::loadMasterPassword();
    bool ok = (g_adminCtx->buf == stored);
    if (ok) {
      closeAdmin(true);
    } else {
      UI::glitchLabel(g_adminCtx->display, "DENIED", 500);
      g_adminCtx->buf = "";
      lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
        if (g_adminCtx) lv_label_set_text(g_adminCtx->display, "_");
        lv_timer_del(tm);
      }, 800, nullptr);
      lv_timer_set_repeat_count(t, 1);
    }
  };
  UI::buildQwerty(kb, cbs);
}

// =========================================================================
//  WIFI CONFIG PAGE (HTML form for backup/restore + master pw change)
// =========================================================================
static const char* HTML_PAGE = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Link Vault</title>
<style>
  body{background:#0a0a0a;color:#ffb000;font-family:'Courier New',monospace;
       max-width:600px;margin:0 auto;padding:16px}
  h1{color:#ff4500;border-bottom:1px solid #663f00;padding-bottom:8px}
  a.btn,button{background:#151515;color:#ffb000;border:1px solid #663f00;
    padding:8px;font-family:inherit;font-size:14px;width:100%;
    box-sizing:border-box;margin:4px 0;display:block;text-align:center;
    text-decoration:none}
  button:hover,a.btn:hover{background:#663f00;cursor:pointer}
  label{color:#663f00;font-size:12px;text-transform:uppercase;letter-spacing:2px;
        display:block;margin-top:8px}
  input{background:#151515;color:#ffb000;border:1px solid #663f00;
    padding:8px;font-family:inherit;font-size:14px;width:100%;
    box-sizing:border-box;margin:4px 0}
</style></head><body>
<h1>// LINK VAULT</h1>
<a class="btn" href="/api/backup">DOWNLOAD BACKUP (vaults.json)</a>
<label>RESTORE FROM FILE</label>
<form method="POST" enctype="multipart/form-data" action="/api/restore">
  <input type="file" name="file" accept="application/json">
  <button type="submit">UPLOAD AND RESTORE</button>
</form>
<label>CHANGE MASTER PASSWORD</label>
<input id="curpw" type="password" placeholder="current master password">
<input id="newpw" type="password" placeholder="new master password (min 4 chars)">
<button onclick="changeMaster()">UPDATE MASTER PW</button>
<script>
async function changeMaster(){
  const current = document.getElementById('curpw').value;
  const pw = document.getElementById('newpw').value;
  if (!current) return alert('enter current password');
  if (pw.length < 4) return alert('new password too short (min 4)');
  const res = await fetch('/api/master', {method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({current, pw})});
  if (res.ok) {
    document.getElementById('curpw').value='';
    document.getElementById('newpw').value='';
    alert('Master password updated');
  } else {
    const j = await res.json().catch(()=>({error:'unknown'}));
    alert('Error: ' + j.error);
  }
}
</script></body></html>
)HTML";

static void startConfigWifi() {
  // Pause BLE while WiFi AP is active — shared radio on ESP32-S3.
  bleProximity_cancelPairing();
  bleKeyboard.end();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  configServer = new AsyncWebServer(80);

  configServer->on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(200, "text/html", HTML_PAGE);
  });
  configServer->on("/api/backup", HTTP_GET, [](AsyncWebServerRequest* req){
    File f = LittleFS.open("/vaults.json", "r");
    if (!f) { req->send(404); return; }
    String s = f.readString();
    f.close();
    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", s);
    resp->addHeader("Content-Disposition", "attachment; filename=\"vaults.json\"");
    req->send(resp);
  });
  configServer->on("/api/restore", HTTP_POST,
    [](AsyncWebServerRequest* req){
      req->send(200, "text/plain", "OK - reload device");
      delay(500);
      ESP.restart();
    },
    [](AsyncWebServerRequest* req, String filename, size_t index, uint8_t* data, size_t len, bool final){
      static File f;
      if (index == 0) f = LittleFS.open("/vaults.json", "w");
      if (f) f.write(data, len);
      if (final && f) f.close();
    });
  configServer->on("/api/master", HTTP_POST,
    [](AsyncWebServerRequest* req){}, NULL,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data, len)) { req->send(400); return; }
      String current = String((const char*)(doc["current"] | ""));
      String pw = String((const char*)(doc["pw"] | ""));
      if (pw.length() < 4) { req->send(400, "application/json", "{\"error\":\"too short\"}"); return; }
      // Verify current master password before allowing change.
      if (Storage::loadMasterPassword() != current) {
        req->send(403, "application/json", "{\"error\":\"wrong current password\"}");
        return;
      }
      Storage::saveMasterPassword(pw);
      req->send(200, "application/json", "{\"ok\":true}");
    });
  configServer->begin();
  configWifiActive = true;
}

static void stopConfigWifi() {
  if (configServer) {
    configServer->end();
    delete configServer;
    configServer = nullptr;
  }
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  configWifiActive = false;

  // Restart BLE after WiFi teardown.
  bleKeyboard.begin();
}
