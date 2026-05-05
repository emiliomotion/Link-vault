/*
 * ui_lock.cpp - Lock screen and all credential-entry sub-screens
 *
 * Screens managed by this file:
 *   STATE_LOCK         - PIN keypad + lockout countdown if active
 *   STATE_MASTER_AUTH  - master password QWERTY (for recovery OR override)
 *   STATE_RECOVERY     - vault name entry after master auth
 *   STATE_NEW_PIN      - 4-digit new PIN after recovery match
 */
#include <lvgl.h>
#include <Arduino.h>
#include "config.h"
#include "theme.h"
#include "app_state.h"
#include "ui_common.h"
#include "ui_qwerty.h"
#include "vaults.h"
#include "security.h"
#include "storage.h"

extern void uiList_show();
extern void uiAwaitProximity_show();

// =========================================================================
//  LOCK SCREEN STATE
// =========================================================================
static lv_obj_t* scrLock;
static lv_obj_t* lockHeader;
static lv_obj_t* lockPinLabel;
static lv_obj_t* lockHint;
static lv_obj_t* lockoutOverlay;       // greys out keypad when locked out
static lv_obj_t* lockoutCountdown;
static lv_obj_t* keypadContainer;
static String pinBuffer = "";
static lv_timer_t* lockoutTickTimer = nullptr;

// =========================================================================
//  MASTER AUTH SCREEN STATE
// =========================================================================
static lv_obj_t* scrMaster;
static lv_obj_t* masterInput;
static String masterBuffer = "";
// Mode: "recovery" (after long-press header) or "override" (proximity bypass)
static enum { MA_RECOVERY, MA_OVERRIDE } masterMode = MA_RECOVERY;

// =========================================================================
//  RECOVERY SCREEN STATE
// =========================================================================
static lv_obj_t* scrRecovery;
static lv_obj_t* recoveryInput;
static String recoveryBuffer = "";
static String recoveryStoredMasterAttempt = "";  // saved from master screen

// =========================================================================
//  NEW PIN SCREEN STATE
// =========================================================================
static lv_obj_t* scrNewPin;
static lv_obj_t* newPinLabel;
static lv_obj_t* newPinHint;
static String newPinBuffer = "";
static Vault* recoveryTargetVault = nullptr;

// Forward declarations within this file
static void renderPinDots();
static void onPinKey(const char* k);
static void buildLockScreen();
static void buildMasterScreen();
static void buildRecoveryScreen();
static void buildNewPinScreen();
static void renderMasterInput();
static void renderRecoveryInput();
static void renderNewPin();
static void onNewPinKey(const char* k);
static void showLockoutCountdown();
static void clearLockoutCountdown();
static void lockoutTickCb(lv_timer_t* t);

// =========================================================================
//  ENTRY POINTS
// =========================================================================
void uiLock_build() {
  buildLockScreen();
  buildMasterScreen();
  buildRecoveryScreen();
  buildNewPinScreen();
}

void uiLock_show() {
  pinBuffer = "";
  renderPinDots();
  App::state = STATE_LOCK;
  App::lastActivityMs = millis();

  if (Security::isLockedOut()) {
    showLockoutCountdown();
  } else {
    clearLockoutCountdown();
  }

  UI::wipeTransition(scrLock);
}

// Show master auth screen in RECOVERY mode (called from header long-press)
static void showMasterAuthForRecovery() {
  masterMode = MA_RECOVERY;
  masterBuffer = "";
  renderMasterInput();
  App::state = STATE_MASTER_AUTH;
  UI::wipeTransition(scrMaster);
}

// Public: show master auth in OVERRIDE mode (proximity bypass)
void uiMasterAuthForOverride_show() {
  masterMode = MA_OVERRIDE;
  masterBuffer = "";
  renderMasterInput();
  App::state = STATE_MASTER_AUTH;
  UI::wipeTransition(scrMaster);
}

// =========================================================================
//  LOCK SCREEN BUILD
// =========================================================================
static void buildLockScreen() {
  scrLock = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scrLock, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scrLock, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scrLock, 0, 0);
  lv_obj_clear_flag(scrLock, LV_OBJ_FLAG_SCROLLABLE);

  // Header (long-press = recovery flow)
  lockHeader = lv_label_create(scrLock);
  lv_label_set_text(lockHeader, "// LINK VAULT :: LOCKED");
  lv_obj_set_style_text_color(lockHeader, COLOR_DIM, 0);
  lv_obj_align(lockHeader, LV_ALIGN_TOP_LEFT, 12, 8);
  lv_obj_add_flag(lockHeader, LV_OBJ_FLAG_CLICKABLE);
  UI::bindLongPress(lockHeader, 700, [](){
    if (Security::isLockedOut()) return;
    showMasterAuthForRecovery();
  });

  // Blinking cursor next to header
  lv_obj_t* cur = lv_obj_create(scrLock);
  lv_obj_set_size(cur, 6, 10);
  lv_obj_set_style_bg_color(cur, COLOR_FG, 0);
  lv_obj_set_style_border_width(cur, 0, 0);
  lv_obj_set_style_radius(cur, 0, 0);
  lv_obj_clear_flag(cur, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align_to(cur, lockHeader, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
  UI::startBlink(cur);

  // PIN dots display
  lockPinLabel = lv_label_create(scrLock);
  lv_label_set_text(lockPinLabel, "- - - -");
  lv_obj_set_style_text_color(lockPinLabel, COLOR_FG, 0);
  lv_obj_set_style_text_font(lockPinLabel, FONT_LARGE, 0);
  lv_obj_align(lockPinLabel, LV_ALIGN_LEFT_MID, 20, 10);

  lockHint = lv_label_create(scrLock);
  lv_label_set_text(lockHint, "> enter pin");
  lv_obj_set_style_text_color(lockHint, COLOR_DIM, 0);
  lv_obj_align(lockHint, LV_ALIGN_LEFT_MID, 20, -20);

  // Keypad (will be hidden during lockout)
  keypadContainer = lv_obj_create(scrLock);
  lv_obj_set_size(keypadContainer, 220, 152);
  lv_obj_set_pos(keypadContainer, 300, 10);
  lv_obj_set_style_bg_opa(keypadContainer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(keypadContainer, 0, 0);
  lv_obj_set_style_pad_all(keypadContainer, 0, 0);
  lv_obj_clear_flag(keypadContainer, LV_OBJ_FLAG_SCROLLABLE);

  const char* keys[12] = {
    "1","2","3", "4","5","6", "7","8","9", "DEL","0","OK"
  };
  for (int i = 0; i < 12; i++) {
    int col = i % 3;
    int row = i / 3;
    lv_obj_t* btn = lv_btn_create(keypadContainer);
    lv_obj_set_size(btn, 68, 36);
    lv_obj_set_pos(btn, col * 72, row * 38);
    lv_obj_set_style_bg_color(btn, COLOR_ROW, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, COLOR_DIM, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t* lab = lv_label_create(btn);
    lv_label_set_text(lab, keys[i]);
    bool isOk = strcmp(keys[i], "OK") == 0;
    lv_obj_set_style_text_color(lab, isOk ? COLOR_ACCENT : COLOR_FG, 0);
    if (isOk) lv_obj_set_style_border_color(btn, COLOR_ACCENT, 0);
    lv_obj_center(lab);

    lv_obj_set_user_data(btn, (void*)keys[i]);
    lv_obj_add_event_cb(btn, [](lv_event_t* e){
      lv_obj_t* btn = lv_event_get_target(e);
      const char* k = (const char*)lv_obj_get_user_data(btn);
      UI::flashKey(btn);
      onPinKey(k);
    }, LV_EVENT_CLICKED, NULL);
  }

  // Lockout overlay (initially hidden)
  lockoutOverlay = lv_obj_create(scrLock);
  lv_obj_set_size(lockoutOverlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(lockoutOverlay, 0, 0);
  lv_obj_set_style_bg_color(lockoutOverlay, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(lockoutOverlay, LV_OPA_90, 0);
  lv_obj_set_style_border_width(lockoutOverlay, 0, 0);
  lv_obj_clear_flag(lockoutOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(lockoutOverlay, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* lockedTitle = lv_label_create(lockoutOverlay);
  lv_label_set_text(lockedTitle, "// LOCKED OUT");
  lv_obj_set_style_text_color(lockedTitle, COLOR_ACCENT, 0);
  lv_obj_set_style_text_font(lockedTitle, FONT_LARGE, 0);
  lv_obj_align(lockedTitle, LV_ALIGN_CENTER, 0, -20);

  lockoutCountdown = lv_label_create(lockoutOverlay);
  lv_obj_set_style_text_color(lockoutCountdown, COLOR_FG, 0);
  lv_label_set_text(lockoutCountdown, "00:00");
  lv_obj_align(lockoutCountdown, LV_ALIGN_CENTER, 0, 20);

  UI::addScanline(scrLock);
}

static void renderPinDots() {
  String dots = "";
  for (uint8_t i = 0; i < PIN_LENGTH; i++) {
    dots += (i < pinBuffer.length()) ? "*" : "-";
    if (i < PIN_LENGTH - 1) dots += " ";
  }
  lv_label_set_text(lockPinLabel, dots.c_str());
  lv_obj_set_style_text_color(lockPinLabel, COLOR_FG, 0);
}

static void resetPinDisplayCb(lv_timer_t* t) {
  renderPinDots();
  lv_timer_del(t);
}

static void onPinKey(const char* k) {
  App::notifyActivity();

  if (Security::isLockedOut()) return;  // ignore input

  if (strcmp(k, "DEL") == 0) {
    if (pinBuffer.length() > 0) pinBuffer.remove(pinBuffer.length() - 1);
  } else if (strcmp(k, "OK") == 0) {
    if (pinBuffer.length() != PIN_LENGTH) return;

    Vault* match = Vaults::checkPin(pinBuffer);
    if (match) {
      // Proximity gate: real (non-decoy) vaults need either proximity OK
      // or override-bypass. Decoy is always allowed (it's the panic surface).
      bool needGate = !match->isDecoy;
      bool gatePassed = App::proximityOK || App::proximityBypassed
                        || Storage::loadBondedAddresses().empty();  // no bond yet = ungated
      if (needGate && !gatePassed) {
        // Phone not present and not overridden — refuse. Move to await screen.
        pinBuffer = "";
        renderPinDots();
        UI::toast("trusted device required", COLOR_ACCENT);
        lv_timer_t* t = lv_timer_create([](lv_timer_t* timer){
          uiAwaitProximity_show();
          lv_timer_del(timer);
        }, 800, nullptr);
        lv_timer_set_repeat_count(t, 1);
        return;
      }

      // Success
      lv_label_set_text(lockPinLabel, "GRANTED");
      lv_obj_set_style_text_color(lockPinLabel, COLOR_GREEN, 0);
      App::enterListScreen(match);
      return;
    } else {
      // Wrong PIN — record, maybe lockout, maybe wipe
      bool shouldWipe = Security::recordFailedAttempt();
      if (shouldWipe) {
        Storage::wipeRealVaults();
        Vaults::begin();   // reload
        UI::toast("REAL VAULTS WIPED", COLOR_ACCENT);
      }
      lv_obj_set_style_text_color(lockPinLabel, COLOR_ACCENT, 0);
      UI::glitchLabel(lockPinLabel, "DENIED", 500);
      pinBuffer = "";
      lv_timer_t* t = lv_timer_create(resetPinDisplayCb, 900, nullptr);
      lv_timer_set_repeat_count(t, 1);

      if (Security::isLockedOut()) {
        showLockoutCountdown();
      }
      return;
    }
  } else {
    if (pinBuffer.length() < PIN_LENGTH) pinBuffer += k;
  }
  renderPinDots();
}

// =========================================================================
//  LOCKOUT COUNTDOWN
// =========================================================================
static void showLockoutCountdown() {
  lv_obj_clear_flag(lockoutOverlay, LV_OBJ_FLAG_HIDDEN);
  if (lockoutTickTimer) lv_timer_del(lockoutTickTimer);
  lockoutTickTimer = lv_timer_create(lockoutTickCb, 1000, nullptr);
}

static void clearLockoutCountdown() {
  lv_obj_add_flag(lockoutOverlay, LV_OBJ_FLAG_HIDDEN);
  if (lockoutTickTimer) { lv_timer_del(lockoutTickTimer); lockoutTickTimer = nullptr; }
}

static void lockoutTickCb(lv_timer_t* t) {
  uint32_t secs = Security::lockoutSecondsLeft();
  if (secs == 0) {
    clearLockoutCountdown();
    return;
  }
  uint32_t mins = secs / 60;
  uint32_t rem = secs % 60;
  char buf[24];
  if (mins >= 60) {
    uint32_t hrs = mins / 60;
    snprintf(buf, sizeof(buf), "%uh %02um", hrs, mins % 60);
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u", mins, rem);
  }
  lv_label_set_text(lockoutCountdown, buf);
}

// =========================================================================
//  MASTER AUTH SCREEN
// =========================================================================
static void buildMasterScreen() {
  scrMaster = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scrMaster, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scrMaster, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scrMaster, 4, 0);
  lv_obj_clear_flag(scrMaster, LV_OBJ_FLAG_SCROLLABLE);

  // Title bar
  lv_obj_t* tb = lv_obj_create(scrMaster);
  lv_obj_set_size(tb, SCREEN_W - 8, 18);
  lv_obj_set_pos(tb, 0, 0);
  lv_obj_set_style_bg_opa(tb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tb, 0, 0);
  lv_obj_set_style_pad_all(tb, 0, 0);
  lv_obj_clear_flag(tb, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(tb);
  lv_label_set_text(title, "// MASTER AUTH");
  lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t* back = lv_btn_create(tb);
  lv_obj_set_size(back, 60, 18);
  lv_obj_align(back, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(back, COLOR_BG, 0);
  lv_obj_set_style_border_width(back, 1, 0);
  lv_obj_set_style_border_color(back, COLOR_FG, 0);
  lv_obj_set_style_radius(back, 0, 0);
  lv_obj_set_style_shadow_width(back, 0, 0);
  lv_obj_t* bl = lv_label_create(back);
  lv_label_set_text(bl, "BACK");
  lv_obj_set_style_text_color(bl, COLOR_FG, 0);
  lv_obj_center(bl);
  lv_obj_add_event_cb(back, [](lv_event_t* e){
    masterBuffer = "";
    uiLock_show();
  }, LV_EVENT_CLICKED, NULL);

  // Password display
  masterInput = lv_label_create(scrMaster);
  lv_obj_set_style_bg_color(masterInput, COLOR_ROW, 0);
  lv_obj_set_style_bg_opa(masterInput, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(masterInput, 1, 0);
  lv_obj_set_style_border_color(masterInput, COLOR_DIM, 0);
  lv_obj_set_style_pad_hor(masterInput, 6, 0);
  lv_obj_set_style_pad_ver(masterInput, 2, 0);
  lv_obj_set_style_text_color(masterInput, COLOR_FG, 0);
  lv_obj_set_width(masterInput, SCREEN_W - 8);
  lv_obj_set_pos(masterInput, 0, 20);
  lv_label_set_text(masterInput, "_");

  // Keyboard container
  lv_obj_t* kb = lv_obj_create(scrMaster);
  lv_obj_set_size(kb, SCREEN_W - 8, SCREEN_H - 44);
  lv_obj_set_pos(kb, 0, 42);
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(kb, 0, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);

  UI::QwertyCallbacks cbs;
  cbs.onChar = [](char c){
    App::notifyActivity();
    if (masterBuffer.length() < 64) {
      masterBuffer += c;
      renderMasterInput();
    }
  };
  cbs.onDelete = [](){
    App::notifyActivity();
    if (masterBuffer.length() > 0) {
      masterBuffer.remove(masterBuffer.length() - 1);
      renderMasterInput();
    }
  };
  cbs.onOk = [](){
    App::notifyActivity();
    String stored = Storage::loadMasterPassword();
    bool correct = (masterBuffer.length() == stored.length());
    if (correct) {
      for (size_t i = 0; i < stored.length(); i++) {
        if (stored[i] != masterBuffer[i]) correct = false;
      }
    }

    if (masterMode == MA_OVERRIDE) {
      // Proximity bypass — succeed/fail loud here is fine because failure
      // doesn't reveal anything sensitive (anyone can try this prompt)
      if (correct) {
        App::proximityBypassed = true;
        masterBuffer = "";
        uiLock_show();  // sets state and handles transition
      } else {
        UI::glitchLabel(masterInput, "DENIED", 500);
        masterBuffer = "";
        // After delay, reset display
        lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
          renderMasterInput();
          lv_timer_del(tm);
        }, 800, nullptr);
        lv_timer_set_repeat_count(t, 1);
      }
      return;
    }

    // RECOVERY mode — never reveal whether pw was correct.
    // Always go to recovery screen; success only happens if pw + name both match.
    recoveryStoredMasterAttempt = masterBuffer;  // remembered silently
    masterBuffer = "";
    recoveryBuffer = "";
    renderRecoveryInput();
    App::state = STATE_RECOVERY;
    UI::wipeTransition(scrRecovery);
  };
  UI::buildQwerty(kb, cbs);
}

static void renderMasterInput() {
  String show = "";
  for (size_t i = 0; i < masterBuffer.length(); i++) show += "*";
  show += "_";
  lv_label_set_text(masterInput, show.c_str());
  lv_obj_set_style_text_color(masterInput, COLOR_FG, 0);
}

// =========================================================================
//  RECOVERY SCREEN
// =========================================================================
static void buildRecoveryScreen() {
  scrRecovery = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scrRecovery, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scrRecovery, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scrRecovery, 4, 0);
  lv_obj_clear_flag(scrRecovery, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* tb = lv_obj_create(scrRecovery);
  lv_obj_set_size(tb, SCREEN_W - 8, 18);
  lv_obj_set_pos(tb, 0, 0);
  lv_obj_set_style_bg_opa(tb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tb, 0, 0);
  lv_obj_set_style_pad_all(tb, 0, 0);
  lv_obj_clear_flag(tb, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(tb);
  lv_label_set_text(title, "// RECOVERY :: identify vault");
  lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 4, 0);

  lv_obj_t* back = lv_btn_create(tb);
  lv_obj_set_size(back, 60, 18);
  lv_obj_align(back, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_style_bg_color(back, COLOR_BG, 0);
  lv_obj_set_style_border_width(back, 1, 0);
  lv_obj_set_style_border_color(back, COLOR_FG, 0);
  lv_obj_set_style_radius(back, 0, 0);
  lv_obj_set_style_shadow_width(back, 0, 0);
  lv_obj_t* bl = lv_label_create(back);
  lv_label_set_text(bl, "BACK");
  lv_obj_set_style_text_color(bl, COLOR_FG, 0);
  lv_obj_center(bl);
  lv_obj_add_event_cb(back, [](lv_event_t* e){
    recoveryBuffer = "";
    recoveryStoredMasterAttempt = "";
    uiLock_show();
  }, LV_EVENT_CLICKED, NULL);

  recoveryInput = lv_label_create(scrRecovery);
  lv_obj_set_style_bg_color(recoveryInput, COLOR_ROW, 0);
  lv_obj_set_style_bg_opa(recoveryInput, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(recoveryInput, 1, 0);
  lv_obj_set_style_border_color(recoveryInput, COLOR_DIM, 0);
  lv_obj_set_style_pad_hor(recoveryInput, 6, 0);
  lv_obj_set_style_pad_ver(recoveryInput, 2, 0);
  lv_obj_set_style_text_color(recoveryInput, COLOR_FG, 0);
  lv_obj_set_width(recoveryInput, SCREEN_W - 8);
  lv_obj_set_pos(recoveryInput, 0, 20);
  lv_label_set_text(recoveryInput, "_");

  lv_obj_t* kb = lv_obj_create(scrRecovery);
  lv_obj_set_size(kb, SCREEN_W - 8, SCREEN_H - 44);
  lv_obj_set_pos(kb, 0, 42);
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(kb, 0, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_SCROLLABLE);

  UI::QwertyCallbacks cbs;
  cbs.onChar = [](char c){
    App::notifyActivity();
    if (recoveryBuffer.length() < 32) {
      recoveryBuffer += c;
      renderRecoveryInput();
    }
  };
  cbs.onDelete = [](){
    App::notifyActivity();
    if (recoveryBuffer.length() > 0) {
      recoveryBuffer.remove(recoveryBuffer.length() - 1);
      renderRecoveryInput();
    }
  };
  cbs.onOk = [](){
    App::notifyActivity();
    String name = recoveryBuffer;
    name.trim();
    name.toLowerCase();

    Vault* v = Vaults::recoveryLookup(name, recoveryStoredMasterAttempt);
    if (v) {
      recoveryTargetVault = v;
      recoveryBuffer = "";
      recoveryStoredMasterAttempt = "";
      newPinBuffer = "";
      renderNewPin();
      App::state = STATE_NEW_PIN;
      UI::wipeTransition(scrNewPin);
    } else {
      // Silent failure
      UI::glitchLabel(recoveryInput, "> no match", 500);
      recoveryBuffer = "";
      lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
        renderRecoveryInput();
        lv_timer_del(tm);
      }, 900, nullptr);
      lv_timer_set_repeat_count(t, 1);
    }
  };
  UI::buildQwerty(kb, cbs);
}

static void renderRecoveryInput() {
  String show = recoveryBuffer + "_";
  lv_label_set_text(recoveryInput, show.c_str());
  lv_obj_set_style_text_color(recoveryInput, COLOR_FG, 0);
}

// =========================================================================
//  NEW PIN SCREEN
// =========================================================================
static void buildNewPinScreen() {
  scrNewPin = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scrNewPin, COLOR_BG, 0);
  lv_obj_set_style_bg_opa(scrNewPin, LV_OPA_COVER, 0);
  lv_obj_clear_flag(scrNewPin, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(scrNewPin);
  lv_label_set_text(title, "// SET NEW PIN FOR VAULT");
  lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 8);

  newPinHint = lv_label_create(scrNewPin);
  lv_label_set_text(newPinHint, "> enter new 4-digit pin");
  lv_obj_set_style_text_color(newPinHint, COLOR_DIM, 0);
  lv_obj_align(newPinHint, LV_ALIGN_LEFT_MID, 20, -20);

  newPinLabel = lv_label_create(scrNewPin);
  lv_label_set_text(newPinLabel, "- - - -");
  lv_obj_set_style_text_color(newPinLabel, COLOR_FG, 0);
  lv_obj_set_style_text_font(newPinLabel, FONT_LARGE, 0);
  lv_obj_align(newPinLabel, LV_ALIGN_LEFT_MID, 20, 10);

  // Numeric keypad on the right
  lv_obj_t* pad = lv_obj_create(scrNewPin);
  lv_obj_set_size(pad, 220, 152);
  lv_obj_set_pos(pad, 300, 10);
  lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(pad, 0, 0);
  lv_obj_set_style_pad_all(pad, 0, 0);
  lv_obj_clear_flag(pad, LV_OBJ_FLAG_SCROLLABLE);

  const char* keys[12] = {
    "1","2","3", "4","5","6", "7","8","9", "DEL","0","OK"
  };
  for (int i = 0; i < 12; i++) {
    int col = i % 3;
    int row = i / 3;
    lv_obj_t* btn = lv_btn_create(pad);
    lv_obj_set_size(btn, 68, 36);
    lv_obj_set_pos(btn, col * 72, row * 38);
    lv_obj_set_style_bg_color(btn, COLOR_ROW, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, COLOR_DIM, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t* lab = lv_label_create(btn);
    lv_label_set_text(lab, keys[i]);
    bool isOk = strcmp(keys[i], "OK") == 0;
    lv_obj_set_style_text_color(lab, isOk ? COLOR_GREEN : COLOR_FG, 0);
    if (isOk) lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
    lv_obj_center(lab);

    lv_obj_set_user_data(btn, (void*)keys[i]);
    lv_obj_add_event_cb(btn, [](lv_event_t* e){
      lv_obj_t* btn = lv_event_get_target(e);
      const char* k = (const char*)lv_obj_get_user_data(btn);
      UI::flashKey(btn);
      onNewPinKey(k);
    }, LV_EVENT_CLICKED, NULL);
  }
}

static void renderNewPin() {
  String dots = "";
  for (uint8_t i = 0; i < PIN_LENGTH; i++) {
    dots += (i < newPinBuffer.length()) ? "*" : "-";
    if (i < PIN_LENGTH - 1) dots += " ";
  }
  lv_label_set_text(newPinLabel, dots.c_str());
  lv_obj_set_style_text_color(newPinLabel, COLOR_FG, 0);
}

static void onNewPinKey(const char* k) {
  App::notifyActivity();
  if (strcmp(k, "DEL") == 0) {
    if (newPinBuffer.length() > 0) newPinBuffer.remove(newPinBuffer.length() - 1);
  } else if (strcmp(k, "OK") == 0) {
    if (newPinBuffer.length() != PIN_LENGTH) return;
    if (!Vaults::pinIsAvailable(newPinBuffer, recoveryTargetVault)) {
      lv_obj_set_style_text_color(newPinLabel, COLOR_ACCENT, 0);
      UI::glitchLabel(newPinLabel, "CONFLICT", 500);
      newPinBuffer = "";
      lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
        renderNewPin();
        lv_timer_del(tm);
      }, 900, nullptr);
      lv_timer_set_repeat_count(t, 1);
      return;
    }
    recoveryTargetVault->pin = newPinBuffer;
    Vaults::persist();
    lv_label_set_text(newPinLabel, "UPDATED");
    lv_obj_set_style_text_color(newPinLabel, COLOR_GREEN, 0);
    newPinBuffer = "";
    lv_timer_t* t = lv_timer_create([](lv_timer_t* tm){
      uiLock_show();
      lv_timer_del(tm);
    }, 900, nullptr);
    lv_timer_set_repeat_count(t, 1);
    return;
  } else {
    if (newPinBuffer.length() < PIN_LENGTH) newPinBuffer += k;
  }
  renderNewPin();
}
