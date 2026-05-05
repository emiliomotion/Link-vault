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
//  SETUP
// =========================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n==========================================");
  Serial.println("==  LINK VAULT boot");
  Serial.println("==========================================");

  // -----------------------------------------------------------------
  // *** WAVESHARE DISPLAY/TOUCH/LVGL INITS GO HERE ***
  // Copy these calls from the Waveshare ESP32-S3-Touch-LCD-3.49
  // demo project. Typical names (varies by demo revision):
  //   axs15231_init();        // display driver
  //   touch_init();           // I2C touch controller
  //   lvgl_init();            // LVGL display & input device registration
  // -----------------------------------------------------------------

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
  delay(5);
}
