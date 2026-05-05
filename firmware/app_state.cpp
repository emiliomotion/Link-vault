/*
 * app_state.cpp
 */
#include "app_state.h"
#include "config.h"
#include "vaults.h"
#include "security.h"
#include "ui_common.h"

namespace App {

AppState state              = STATE_BOOT;
Vault*   currentVault       = nullptr;
String   activeCategory     = "ALL";
uint32_t lastActivityMs     = 0;
bool     proximityOK        = false;
bool     proximityBypassed  = false;

void notifyActivity() {
  lastActivityMs = millis();
}

}  // namespace App
