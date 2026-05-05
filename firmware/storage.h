/*
 * storage.h - LittleFS-backed persistent storage for vaults, settings, security state.
 *
 * On-disk layout (all under LittleFS root):
 *   /vaults.json    - all vaults (links, categories, settings, pin)
 *   /master.txt     - master password (changeable via admin)
 *   /security.json  - failed attempt count, lockout-until timestamp, tamper info
 *   /global.json    - global settings (proximity override, wipe threshold, etc.)
 *   /bonds.json     - list of bonded BLE phone addresses
 */
#pragma once
#include <Arduino.h>
#include <vector>

struct Link {
  String name;
  String url;
  String cat;        // category name
  uint32_t lastUsed; // millis since epoch (cheap "last used" tracking)
  uint32_t timesUsed;
};

struct Category {
  String name;
  uint32_t color;    // 0xRRGGBB
};

struct VaultSettings {
  bool autoEnter;       // press Enter after typing URL
  uint8_t sleepMinutes; // deep sleep timeout (must be in SLEEP_CYCLE_MIN)
};

struct Vault {
  String name;          // displayed name. NEVER use "decoy" as name; pick innocuous
  String pin;           // 4-digit string
  bool isDecoy;
  std::vector<Category> categories;
  std::vector<Link> links;
  VaultSettings settings;
};

struct GlobalSettings {
  bool proximityOverrideEnabled;  // master pw can bypass proximity check
  uint8_t wipeThreshold;          // 0 = disabled
  uint32_t lastUnlockTime;        // millis (set on every successful unlock)
  String lastUnlockedVault;       // for tamper indicator: "you unlocked X 2 hours ago"
};

struct SecurityState {
  uint8_t failedAttempts;         // resets on any successful unlock
  uint32_t lockoutUntilEpoch;     // unix-ish seconds when lockout ends; 0 = no lockout
  uint32_t totalFailedSinceLastUnlock;  // for tamper indicator
};

namespace Storage {
  // Initialize LittleFS, run first-boot seeding if needed.
  bool begin();

  // Vaults
  bool loadVaults(std::vector<Vault>& out);
  bool saveVaults(const std::vector<Vault>& vaults);

  // Master password
  String loadMasterPassword();
  bool saveMasterPassword(const String& pw);

  // Global settings
  GlobalSettings loadGlobalSettings();
  bool saveGlobalSettings(const GlobalSettings& s);

  // Security state (failed attempts, lockout)
  SecurityState loadSecurityState();
  bool saveSecurityState(const SecurityState& s);

  // BLE bonds
  std::vector<String> loadBondedAddresses();
  bool saveBondedAddresses(const std::vector<String>& addrs);
  bool addBondedAddress(const String& addr);
  bool removeBondedAddress(const String& addr);

  // Wipe REAL vaults (decoy survives). For wipe-on-N-attempts feature.
  bool wipeRealVaults();

  // Full factory reset
  bool factoryReset();
}
