/*
 * =========================================================================
 *  LINK VAULT  -  Waveshare ESP32-S3-Touch-LCD-3.49 (640x172)
 * =========================================================================
 *  Multi-vault PIN-locked URL store with cypherpunk UI, BLE HID typing,
 *  proximity-gated unlock, decoy vault, persistent brute-force backoff.
 *
 *  See README.md for full build instructions, library list, hosting-free
 *  pairing flow, and Waveshare display driver integration steps.
 * =========================================================================
 */

#include <Arduino.h>
#include <lvgl.h>
#include <BleKeyboard.h>

#include "config.h"
#include "theme.h"
#include "storage.h"
#include "vaults.h"
#include "security.h"
#include "app_state.h"
#include "ui_common.h"
#include "ui_qwerty.h"

// Waveshare ESP32-S3-Touch-LCD-3.49 driver files (copy from demo project)
#include "user_config.h"
#include "lvgl_port.h"
#include "i2c_bsp.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"

// --- Forward declarations from screen modules (Message 2 of 3) -----------
extern void uiBoot_build();
extern void uiBoot_show();
extern void uiLock_build();
extern void uiLock_show();
extern void uiList_build();
extern void uiList_show();
extern void uiConfig_build();
extern void uiConfig_show();
extern void uiAwaitProximity_show();

extern void bleProximity_begin();
extern void bleProximity_tick();   // call from loop()
extern bool bleProximity_phonePresent();
extern void bleProximity_startPairing();   // user-initiated pair flow
extern bool bleProximity_isPairing();
// -------------------------------------------------------------------------

// Global BLE keyboard (used for typing URLs to the phone)
BleKeyboard bleKeyboard(BLE_DEVICE_NAME, BLE_MANUFACTURER, 100);

// =========================================================================
//  App::lockNow / App::enterListScreen
//  Defined here because they cross multiple modules.
// =========================================================================
namespace App {
  void lockNow() {
    currentVault = nullptr;
    activeCategory = "ALL";
    proximityBypassed = false;
    state = STATE_LOCK;
    uiLock_show();
  }

  void enterListScreen(Vault* v) {
    currentVault = v;
    activeCategory = "ALL";
    state = STATE_LIST;
    Security::recordSuccessfulUnlock(*v);
    uiList_show();
  }
}

// =========================================================================
//  Auto-lock idle watchdog
// =========================================================================
static void checkAutoLock() {
  if (App::state != STATE_LIST && App::state != STATE_CONFIG) return;
  if (!App::currentVault) return;
  // Use per-vault sleepMinutes; fall back to 5 min if somehow 0.
  uint8_t mins = App::currentVault->settings.sleepMinutes;
  if (mins == 0) mins = 5;
  uint32_t timeoutMs = (uint32_t)mins * 60000UL;
  if (millis() - App::lastActivityMs > timeoutMs) {
    App::lockNow();
  }
}

// =========================================================================
//  PANIC CLOSE — double-tap the vault title → sends Ctrl+W to close the
//  active incognito tab on whatever device is currently connected via BLE.
//  Works on Windows (Chrome/Edge) and Android (Chrome, one tab).
//  The topbar title zone (x<200, y<22) has no buttons so accidental
//  triggers from normal use are not possible.
//  NOTE: the CST816S touch IC on this board is single-touch only, so
//  two-finger simultaneous detection is not possible in hardware.
// =========================================================================
static uint32_t s_panicLastTapMs  = 0;
static bool     s_panicPrevPressed = false;

static void doPanicClose() {
  if (!bleKeyboard.isConnected()) {
    UI::toast("!! BLE OFFLINE !!", COLOR_ACCENT);
    return;
  }
  UI::toast("// INCOGNITO CLOSED", COLOR_GREEN);
  bleKeyboard.press(KEY_LEFT_CTRL);
  bleKeyboard.press('w');
  bleKeyboard.releaseAll();
}

static void checkPanicGesture() {
  if (App::state != STATE_LIST && App::state != STATE_CONFIG) {
    s_panicPrevPressed = false;
    return;
  }

  lv_indev_t* indev = lv_indev_get_act();
  if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_POINTER) {
    s_panicPrevPressed = false;
    return;
  }

  lv_point_t pt;
  lv_indev_get_point(indev, &pt);
  bool pressed = true; // lv_indev_get_act() only returns non-null when actively pressed

  if (pressed && !s_panicPrevPressed && pt.x < 200 && pt.y < 22) {
    uint32_t now = millis();
    if (now - s_panicLastTapMs < 350) {
      s_panicLastTapMs = 0;
      s_panicPrevPressed = pressed;
      doPanicClose();
      return;
    }
    s_panicLastTapMs = now;
  }
  s_panicPrevPressed = pressed;
}

// =========================================================================
//  SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n==========================================");
  Serial.println("==  LINK VAULT boot");
  Serial.println("==========================================");

  // Waveshare display/touch/LVGL initialization
  i2c_master_Init();
  lvgl_port_init();
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);

  if (!Storage::begin()) {
    Serial.println("[FATAL] Storage init failed");
    while (true) { delay(1000); }
  }

  Vaults::begin();
  Security::begin();

  // BLE HID must initialize NimBLE first; proximity scanner attaches on top.
  bleKeyboard.begin();
  bleProximity_begin();

  // Build all UI screens (they're hidden until shown)
  uiBoot_build();
  uiLock_build();
  uiList_build();
  uiConfig_build();

  // Start with boot animation. uiBoot_show() will call uiAwaitProximity_show()
  // when the boot sequence completes.
  uiBoot_show();
  App::lastActivityMs = millis();

  Serial.println("[OK] setup() complete");
}

// =========================================================================
//  LOOP
// =========================================================================
void loop() {
  lv_timer_handler();
  bleProximity_tick();
  checkAutoLock();
  checkPanicGesture();
  delay(5);
}
