/*
 * app_state.h - Shared global application state.
 * The various UI files reference these to coordinate without circular includes.
 */
#pragma once
#include <Arduino.h>
#include "storage.h"

enum AppState {
  STATE_BOOT,
  STATE_AWAIT_PROXIMITY, // waiting for bonded phone
  STATE_LOCK,
  STATE_MASTER_AUTH,     // master pw entry (recovery flow)
  STATE_RECOVERY,        // typing vault name to recover
  STATE_NEW_PIN,         // setting new PIN after recovery
  STATE_LIST,
  STATE_CONFIG
};

namespace App {
  extern AppState state;
  extern Vault*   currentVault;       // pointer into Vaults::all()
  extern String   activeCategory;     // "ALL" or category name
  extern uint32_t lastActivityMs;
  extern bool     proximityOK;        // true if bonded phone seen recently
  extern bool     proximityBypassed;  // true if user used master pw override

  void notifyActivity();              // call from any user input
  void lockNow();                     // immediate transition to lock screen
  void enterListScreen(Vault* v);     // post-unlock entry point
}
