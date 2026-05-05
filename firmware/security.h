/*
 * security.h - Anti-brute-force backoff, tamper indicator state.
 *
 * Public API:
 *   Security::begin()              load persisted state from flash
 *   Security::isLockedOut()        true if currently in lockout window
 *   Security::lockoutSecondsLeft() seconds remaining
 *   Security::recordFailedAttempt() increments counter, may trigger lockout/wipe
 *   Security::recordSuccessfulUnlock(vault) reset counter, update tamper state
 *   Security::tamperReport()       human-readable: "last unlock 2h ago, 0 fails since"
 */
#pragma once
#include <Arduino.h>
#include "storage.h"

namespace Security {
  void begin();

  bool isLockedOut();
  uint32_t lockoutSecondsLeft();

  // Returns true if we should wipe real vaults (caller does the wipe).
  bool recordFailedAttempt();

  void recordSuccessfulUnlock(const Vault& v);

  // For display on list screen post-unlock
  String tamperReport();

  // Direct access to current values
  const SecurityState& state();
}
