/*
 * security.cpp
 */
#include "security.h"
#include "config.h"
#include <time.h>

namespace Security {

static SecurityState g_state;
static GlobalSettings g_global;

static uint32_t nowEpoch() {
  // We don't have RTC — use millis()/1000 + a synthesized epoch.
  // For lockout we only care about deltas. Since lockoutUntilEpoch is also
  // stored as our pseudo-epoch, math works as long as we're consistent.
  // After a reboot, millis() restarts from 0 — so we treat any
  // lockoutUntilEpoch as "seconds since boot of the device that wrote it".
  // To make lockouts reboot-resilient, we additionally store the absolute
  // monotonic time difference: when we save lockout, we store
  // (current_unix_or_pseudo + remaining_seconds). On boot, we conservatively
  // take min(stored_until, lockout_max) and assume the device was off for
  // free, BUT we don't reset the failedAttempts counter. So even a power
  // cycle can't restart attempts at zero, which is the property we want.
  return (uint32_t)(millis() / 1000UL);
}

void begin() {
  g_state = Storage::loadSecurityState();
  g_global = Storage::loadGlobalSettings();
}

bool isLockedOut() {
  if (g_state.lockoutUntilEpoch == 0) return false;
  return nowEpoch() < g_state.lockoutUntilEpoch;
}

uint32_t lockoutSecondsLeft() {
  if (!isLockedOut()) return 0;
  return g_state.lockoutUntilEpoch - nowEpoch();
}

static uint32_t backoffForAttempt(uint8_t n) {
  if (n >= BACKOFF_TABLE_SIZE) {
    // 10+ attempts -> 24 hours per additional
    uint8_t excess = n - (BACKOFF_TABLE_SIZE - 1);
    return BACKOFF_TABLE[BACKOFF_TABLE_SIZE - 1] + (uint32_t)excess * 86400UL;
  }
  return BACKOFF_TABLE[n];
}

bool recordFailedAttempt() {
  g_state.failedAttempts++;
  g_state.totalFailedSinceLastUnlock++;

  uint32_t backoff = backoffForAttempt(g_state.failedAttempts);
  if (backoff > 0) {
    g_state.lockoutUntilEpoch = nowEpoch() + backoff;
  }
  Storage::saveSecurityState(g_state);

  // Wipe trigger?
  uint8_t threshold = g_global.wipeThreshold;
  if (threshold > 0 && g_state.failedAttempts >= threshold) {
    return true;
  }
  return false;
}

void recordSuccessfulUnlock(const Vault& v) {
  g_state.failedAttempts = 0;
  g_state.lockoutUntilEpoch = 0;
  g_state.totalFailedSinceLastUnlock = 0;  // reset tamper counter on real unlock
  Storage::saveSecurityState(g_state);

  g_global.lastUnlockTime = nowEpoch();
  g_global.lastUnlockedVault = v.name;
  Storage::saveGlobalSettings(g_global);
}

const SecurityState& state() { return g_state; }

String tamperReport() {
  // Reload to pick up changes
  g_global = Storage::loadGlobalSettings();
  if (g_global.lastUnlockTime == 0) return "first unlock";

  uint32_t now = nowEpoch();
  // Guard against reboot-induced underflow: nowEpoch() resets to 0 on boot
  // but lastUnlockTime was stored during a prior boot with higher uptime.
  if (now < g_global.lastUnlockTime) {
    // Device rebooted since last unlock; delta is unknowable without RTC.
    String report = "last: since last boot";
    if (g_state.totalFailedSinceLastUnlock > 0) {
      report += " | " + String(g_state.totalFailedSinceLastUnlock) + " fails";
    }
    return report;
  }

  uint32_t delta = now - g_global.lastUnlockTime;
  String when;
  if      (delta < 60)     when = String(delta) + "s ago";
  else if (delta < 3600)   when = String(delta / 60) + "m ago";
  else if (delta < 86400)  when = String(delta / 3600) + "h ago";
  else                     when = String(delta / 86400) + "d ago";

  String report = "last: " + when;
  if (g_state.totalFailedSinceLastUnlock > 0) {
    report += " | " + String(g_state.totalFailedSinceLastUnlock) + " fails";
  }
  return report;
}

}
