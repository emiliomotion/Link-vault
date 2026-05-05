/*
 * vaults.cpp - vault list management
 */
#include "vaults.h"
#include "config.h"

namespace Vaults {

static std::vector<Vault> g_vaults;

void begin() {
  Storage::loadVaults(g_vaults);
}

std::vector<Vault>& all() { return g_vaults; }

Vault* findByName(const String& name) {
  for (auto& v : g_vaults) if (v.name == name) return &v;
  return nullptr;
}

Vault* findByPin(const String& pin) {
  for (auto& v : g_vaults) if (v.pin == pin) return &v;
  return nullptr;
}

void persist() { Storage::saveVaults(g_vaults); }

Vault* checkPin(const String& pin) {
  return findByPin(pin);
}

Vault* recoveryLookup(const String& name, const String& masterPw) {
  String stored = Storage::loadMasterPassword();
  // Constant-time-ish compare to avoid timing leaks (low-stakes here but cheap)
  if (stored.length() != masterPw.length()) return nullptr;
  bool match = true;
  for (size_t i = 0; i < stored.length(); i++) {
    if (stored[i] != masterPw[i]) match = false;
  }
  if (!match) return nullptr;
  Vault* v = findByName(name);
  if (!v) return nullptr;
  if (v->isDecoy) return nullptr;   // decoy is NOT recoverable
  return v;
}

bool pinIsAvailable(const String& pin, const Vault* exclude) {
  for (auto& v : g_vaults) {
    if (&v == exclude) continue;
    if (v.pin == pin) return false;
  }
  return true;
}

bool createVault(const String& name, const String& pin) {
  if (g_vaults.size() >= MAX_VAULTS) return false;
  if (!pinIsAvailable(pin)) return false;
  if (findByName(name)) return false;
  Vault v;
  v.name = name;
  v.pin = pin;
  v.isDecoy = false;
  v.categories.push_back({"main", 0xFFB000});
  v.settings = { true, 5 };
  g_vaults.push_back(v);
  persist();
  return true;
}

}
