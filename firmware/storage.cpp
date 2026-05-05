/*
 * storage.cpp - LittleFS-backed storage implementation
 */
#include "storage.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace Storage {

static const char* PATH_VAULTS    = "/vaults.json";
static const char* PATH_MASTER    = "/master.txt";
static const char* PATH_GLOBAL    = "/global.json";
static const char* PATH_SECURITY  = "/security.json";
static const char* PATH_BONDS     = "/bonds.json";

// --------------------------------------------------------------- helpers
static String readAll(const char* path) {
  if (!LittleFS.exists(path)) return "";
  File f = LittleFS.open(path, "r");
  if (!f) return "";
  String s = f.readString();
  f.close();
  return s;
}

static bool writeAll(const char* path, const String& contents) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(contents);
  f.close();
  return true;
}

// --------------------------------------------------------------- first boot seeding
static void seedFirstBoot() {
  Serial.println("[Storage] First boot - seeding defaults");

  // Master password
  writeAll(PATH_MASTER, MASTER_PASSWORD);

  // Default vaults: 3 real + 1 decoy (decoy named innocuously)
  std::vector<Vault> v;

  Vault personal;
  personal.name = "personal"; personal.pin = DEFAULT_PIN_PERSONAL; personal.isDecoy = false;
  personal.categories.push_back({"daily",   0xFFB000});
  personal.categories.push_back({"reading", 0x00FF41});
  personal.settings = { true, 5 };
  v.push_back(personal);

  Vault work;
  work.name = "work"; work.pin = DEFAULT_PIN_WORK; work.isDecoy = false;
  work.categories.push_back({"clients",  0xFFB000});
  work.categories.push_back({"internal", 0x00FF41});
  work.settings = { true, 5 };
  v.push_back(work);

  Vault research;
  research.name = "research"; research.pin = DEFAULT_PIN_RESEARCH; research.isDecoy = false;
  research.categories.push_back({"papers", 0xFFB000});
  research.categories.push_back({"blogs",  0x00FF41});
  research.settings = { true, 5 };
  v.push_back(research);

  // Decoy - innocuous name, pre-seeded with believable throwaway links
  Vault decoy;
  decoy.name = "bookmarks"; decoy.pin = DEFAULT_PIN_DECOY; decoy.isDecoy = true;
  decoy.categories.push_back({"misc",   0xFFB000});
  decoy.categories.push_back({"videos", 0x00FF41});
  decoy.links.push_back({"wikipedia", "https://wikipedia.org",   "misc",   0, 0});
  decoy.links.push_back({"weather",   "https://weather.com",     "misc",   0, 0});
  decoy.links.push_back({"youtube",   "https://youtube.com",     "videos", 0, 0});
  decoy.links.push_back({"maps",      "https://maps.google.com", "misc",   0, 0});
  decoy.links.push_back({"amazon",    "https://amazon.com",      "misc",   0, 0});
  decoy.settings = { true, 5 };
  v.push_back(decoy);

  saveVaults(v);

  // Global settings
  GlobalSettings g;
  g.proximityOverrideEnabled = DEFAULT_PROXIMITY_OVERRIDE_ENABLED;
  g.wipeThreshold = DEFAULT_WIPE_THRESHOLD;
  g.lastUnlockTime = 0;
  g.lastUnlockedVault = "";
  saveGlobalSettings(g);

  // Security state
  SecurityState s = { 0, 0, 0 };
  saveSecurityState(s);

  // Bonds: empty until user pairs phone
  std::vector<String> bonds;
  saveBondedAddresses(bonds);
}

bool begin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[Storage] LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists(PATH_MASTER)) {
    seedFirstBoot();
  }
  return true;
}

// --------------------------------------------------------------- vaults
bool loadVaults(std::vector<Vault>& out) {
  out.clear();
  String s = readAll(PATH_VAULTS);
  if (s.length() == 0) return false;

  DynamicJsonDocument doc(32 * 1024);
  if (deserializeJson(doc, s)) return false;

  for (JsonObject vo : doc["vaults"].as<JsonArray>()) {
    Vault v;
    v.name    = String((const char*)vo["name"]);
    v.pin     = String((const char*)vo["pin"]);
    v.isDecoy = vo["isDecoy"] | false;
    v.settings.autoEnter    = vo["settings"]["autoEnter"]    | true;
    v.settings.sleepMinutes = vo["settings"]["sleepMinutes"] | 5;

    for (JsonObject co : vo["categories"].as<JsonArray>()) {
      Category c;
      c.name  = String((const char*)co["name"]);
      c.color = co["color"] | 0xFFB000;
      v.categories.push_back(c);
    }
    for (JsonObject lo : vo["links"].as<JsonArray>()) {
      Link l;
      l.name      = String((const char*)lo["name"]);
      l.url       = String((const char*)lo["url"]);
      l.cat       = String((const char*)lo["cat"]);
      l.lastUsed  = lo["lastUsed"]  | 0;
      l.timesUsed = lo["timesUsed"] | 0;
      v.links.push_back(l);
    }
    out.push_back(v);
  }
  return true;
}

bool saveVaults(const std::vector<Vault>& vaults) {
  DynamicJsonDocument doc(32 * 1024);
  JsonArray arr = doc.createNestedArray("vaults");
  for (const auto& v : vaults) {
    JsonObject vo = arr.createNestedObject();
    vo["name"]    = v.name;
    vo["pin"]     = v.pin;
    vo["isDecoy"] = v.isDecoy;
    JsonObject so = vo.createNestedObject("settings");
    so["autoEnter"]    = v.settings.autoEnter;
    so["sleepMinutes"] = v.settings.sleepMinutes;

    JsonArray cats = vo.createNestedArray("categories");
    for (const auto& c : v.categories) {
      JsonObject co = cats.createNestedObject();
      co["name"]  = c.name;
      co["color"] = c.color;
    }
    JsonArray lnks = vo.createNestedArray("links");
    for (const auto& l : v.links) {
      JsonObject lo = lnks.createNestedObject();
      lo["name"]      = l.name;
      lo["url"]       = l.url;
      lo["cat"]       = l.cat;
      lo["lastUsed"]  = l.lastUsed;
      lo["timesUsed"] = l.timesUsed;
    }
  }
  String out;
  serializeJson(doc, out);
  return writeAll(PATH_VAULTS, out);
}

// --------------------------------------------------------------- master pw
String loadMasterPassword() {
  String s = readAll(PATH_MASTER);
  s.trim();
  return s;
}

bool saveMasterPassword(const String& pw) {
  return writeAll(PATH_MASTER, pw);
}

// --------------------------------------------------------------- global settings
GlobalSettings loadGlobalSettings() {
  GlobalSettings g;
  g.proximityOverrideEnabled = DEFAULT_PROXIMITY_OVERRIDE_ENABLED;
  g.wipeThreshold = DEFAULT_WIPE_THRESHOLD;
  g.lastUnlockTime = 0;

  String s = readAll(PATH_GLOBAL);
  if (s.length() == 0) return g;
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, s)) return g;
  g.proximityOverrideEnabled = doc["proxOverride"] | DEFAULT_PROXIMITY_OVERRIDE_ENABLED;
  g.wipeThreshold = doc["wipeThreshold"] | DEFAULT_WIPE_THRESHOLD;
  g.lastUnlockTime = doc["lastUnlock"] | 0;
  g.lastUnlockedVault = String((const char*)(doc["lastVault"] | ""));
  return g;
}

bool saveGlobalSettings(const GlobalSettings& s) {
  StaticJsonDocument<512> doc;
  doc["proxOverride"]  = s.proximityOverrideEnabled;
  doc["wipeThreshold"] = s.wipeThreshold;
  doc["lastUnlock"]    = s.lastUnlockTime;
  doc["lastVault"]     = s.lastUnlockedVault;
  String out;
  serializeJson(doc, out);
  return writeAll(PATH_GLOBAL, out);
}

// --------------------------------------------------------------- security state
SecurityState loadSecurityState() {
  SecurityState s = { 0, 0, 0 };
  String txt = readAll(PATH_SECURITY);
  if (txt.length() == 0) return s;
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, txt)) return s;
  s.failedAttempts = doc["fails"] | 0;
  s.lockoutUntilEpoch = doc["until"] | 0;
  s.totalFailedSinceLastUnlock = doc["totalSince"] | 0;
  return s;
}

bool saveSecurityState(const SecurityState& s) {
  StaticJsonDocument<256> doc;
  doc["fails"] = s.failedAttempts;
  doc["until"] = s.lockoutUntilEpoch;
  doc["totalSince"] = s.totalFailedSinceLastUnlock;
  String out;
  serializeJson(doc, out);
  return writeAll(PATH_SECURITY, out);
}

// --------------------------------------------------------------- bonds
std::vector<String> loadBondedAddresses() {
  std::vector<String> out;
  String s = readAll(PATH_BONDS);
  if (s.length() == 0) return out;
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, s)) return out;
  for (JsonVariant v : doc["addrs"].as<JsonArray>()) {
    out.push_back(String((const char*)v));
  }
  return out;
}

bool saveBondedAddresses(const std::vector<String>& addrs) {
  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.createNestedArray("addrs");
  for (const auto& a : addrs) arr.add(a);
  String out;
  serializeJson(doc, out);
  return writeAll(PATH_BONDS, out);
}

bool addBondedAddress(const String& addr) {
  auto list = loadBondedAddresses();
  for (const auto& a : list) if (a == addr) return true;  // already there
  list.push_back(addr);
  return saveBondedAddresses(list);
}

bool removeBondedAddress(const String& addr) {
  auto list = loadBondedAddresses();
  std::vector<String> filtered;
  for (const auto& a : list) if (a != addr) filtered.push_back(a);
  return saveBondedAddresses(filtered);
}

// --------------------------------------------------------------- wipe
bool wipeRealVaults() {
  std::vector<Vault> v;
  if (!loadVaults(v)) return false;
  std::vector<Vault> kept;
  for (const auto& vlt : v) {
    if (vlt.isDecoy) kept.push_back(vlt);
  }
  return saveVaults(kept);
}

bool factoryReset() {
  LittleFS.remove(PATH_VAULTS);
  LittleFS.remove(PATH_MASTER);
  LittleFS.remove(PATH_GLOBAL);
  LittleFS.remove(PATH_SECURITY);
  LittleFS.remove(PATH_BONDS);
  seedFirstBoot();
  return true;
}

}  // namespace Storage
