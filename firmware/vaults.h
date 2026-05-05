/*
 * vaults.h - In-memory vault list & operations.
 * Wraps Storage with a runtime-mutable list of vaults.
 */
#pragma once
#include "storage.h"
#include <vector>

namespace Vaults {

  // Initialize: load vaults from storage into RAM
  void begin();

  // Mutators (these persist immediately to flash)
  std::vector<Vault>& all();
  Vault* findByName(const String& name);
  Vault* findByPin(const String& pin);   // null if no match
  void persist();                        // save current in-memory state to flash

  // Returns the matching vault and sets *isDecoy if PIN is the panic PIN.
  // Returns nullptr if no vault has that PIN.
  Vault* checkPin(const String& pin);

  // Recovery: validate name+master pw, return vault if valid AND not decoy
  Vault* recoveryLookup(const String& name, const String& masterPw);

  // Add a new vault (used in admin "create new vault"). Returns false if
  // limit reached or PIN conflicts.
  bool createVault(const String& name, const String& pin);

  // PIN conflict check (used when changing vault PIN or recovery)
  bool pinIsAvailable(const String& pin, const Vault* exclude = nullptr);
}
