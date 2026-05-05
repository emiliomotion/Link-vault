/*
 * config.h - All tuneable constants for Link Vault
 * Edit values here, not scattered throughout the code.
 */
#pragma once
#include <Arduino.h>

// =================== HARDCODED CREDENTIALS (first boot defaults) ===================
// On first boot, the device seeds these as defaults. Once you change them via
// the admin UI, the new values are stored in LittleFS and these defaults are
// no longer used.

#define DEFAULT_PIN_PERSONAL   "1234"
#define DEFAULT_PIN_WORK       "2580"
#define DEFAULT_PIN_RESEARCH   "1111"
#define DEFAULT_PIN_DECOY      "7851"   // PANIC PIN - opens decoy vault
#define MASTER_PASSWORD        "Esoteric97!"

// =================== BLE / PAIRING ===================
#define BLE_DEVICE_NAME        "Link Vault"
#define BLE_MANUFACTURER       "LinkVault"

// Proximity scan: how many seconds we look for the bonded phone on wake
#define PROXIMITY_SCAN_SEC     2
// RSSI threshold (dBm). -75 = roughly arm's reach. Lower (more negative) = farther.
#define PROXIMITY_RSSI_MIN     -75
// Default for "master password override when phone not present" — user can change in admin
#define DEFAULT_PROXIMITY_OVERRIDE_ENABLED  true

// =================== TIMEOUTS ===================
#define AUTO_LOCK_MS           45000UL    // re-lock after this many ms idle on list/config
#define DECOY_BADGE_MS         5000UL     // how long the [DECOY] badge shows on long-press
#define WIPE_TRANSITION_MS     180        // screen wipe duration (each phase)
#define KEY_FLASH_MS           180        // keystroke flash fade-out
#define TOAST_MS               1500       // ephemeral toast lifetime

// Deep sleep cycle (tap to advance through these in config)
static const uint16_t SLEEP_CYCLE_MIN[] = { 1, 5, 15, 30 };
#define SLEEP_CYCLE_COUNT      4

// =================== BRUTE FORCE BACKOFF (seconds) ===================
// Wrong-attempt count -> lockout duration in seconds.
// Counter resets on any successful unlock (real or decoy).
// Persists across reboots in LittleFS.
static const uint32_t BACKOFF_TABLE[] = {
  0,        // 0 attempts: no lockout (this slot unused)
  0,        // 1 wrong:    no lockout
  0,        // 2 wrong:    no lockout
  30,       // 3 wrong:    30 sec
  60,       // 4 wrong:    1 min
  300,      // 5 wrong:    5 min
  900,      // 6 wrong:    15 min
  1800,     // 7 wrong:    30 min
  3600,     // 8 wrong:    1 hour
  21600,    // 9 wrong:    6 hours
  86400,    // 10+ wrong:  24 hours per additional
};
#define BACKOFF_TABLE_SIZE    11

// Optional: wipe real vaults after this many wrong attempts (decoy survives)
// Default OFF — set to 0 to disable.
#define DEFAULT_WIPE_THRESHOLD 0

// =================== UI ===================
#define SCREEN_W              640
#define SCREEN_H              172
#define MAX_VAULTS            4
#define MAX_LINKS_PER_VAULT   200
#define MAX_CATEGORIES        12
#define PIN_LENGTH            4

// =================== WIFI CONFIG MODE ===================
#define AP_SSID               "LinkVault-Setup"
#define AP_PASSWORD           "vaultopen"   // >= 8 chars
