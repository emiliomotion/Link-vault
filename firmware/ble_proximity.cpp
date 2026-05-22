/*
 * ble_proximity.cpp
 *
 * Implementation strategy:
 *  - Use NimBLE-Arduino directly (it's pulled in as a dep of ESP32-BLE-Keyboard).
 *  - Maintain a passive scanner that runs ~1s every PROXIMITY_INTERVAL_MS.
 *  - During a scan, if any advertised address matches a bonded MAC, refresh
 *    the "last seen" timestamp.
 *  - phonePresent() returns true when (millis() - lastSeen) < GRACE_MS AND
 *    last RSSI was >= PROXIMITY_RSSI_MIN.
 *  - Pairing mode briefly switches advertising to "Numeric Comparison"
 *    capable mode, displays a 6-digit confirmation code on the screen, and
 *    on successful bond, stores the peer's MAC.
 *
 * IMPORTANT: this implementation assumes NimBLE-Arduino >= 1.4.x.
 * On the Waveshare ESP32-S3 demo, NimBLE is already initialized when
 * BleKeyboard::begin() is called; we just attach a scanner on top.
 */
#include "ble_proximity.h"
#include "config.h"
#include "storage.h"
#include <Arduino.h>
#include <vector>

// NimBLE headers - included via ESP32-BLE-Keyboard transitively
#include <NimBLEDevice.h>

// =========================================================================
//  STATE
// =========================================================================
#define PROXIMITY_INTERVAL_MS  3000    // scan every 3 sec
#define PROXIMITY_SCAN_TIME_MS 1500    // each scan lasts 1.5 sec
#define PROXIMITY_GRACE_MS    20000    // phone considered present 20s after last sight

static std::vector<String> g_bonded;
static uint32_t g_lastSeenMs = 0;
static int8_t   g_lastRssi   = -127;
static uint32_t g_lastScanMs = 0;
static bool     g_scanning   = false;

// Pairing
static bool     g_pairing       = false;
static uint32_t g_pairCode      = 0;
static uint32_t g_pairStartMs   = 0;
#define PAIRING_TIMEOUT_MS 60000UL

// =========================================================================
//  SCAN CALLBACK
// =========================================================================
class ProximityScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    String mac = String(dev->getAddress().toString().c_str());
    mac.toLowerCase();
    int8_t rssi = dev->getRSSI();

    for (const auto& bonded : g_bonded) {
      String b = bonded; b.toLowerCase();
      if (mac == b) {
        if (rssi >= PROXIMITY_RSSI_MIN) {
          g_lastSeenMs = millis();
          g_lastRssi = rssi;
        }
        return;
      }
    }
  }
};
static ProximityScanCallbacks scanCb;

// =========================================================================
//  PAIRING CALLBACK
// =========================================================================
class PairingServerCallbacks : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    if (!g_pairing) return;
    String mac = String(connInfo.getAddress().toString().c_str());
    Serial.printf("[Pair] Incoming connect from %s\n", mac.c_str());
  }

  void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
    if (!connInfo.isBonded()) {
      Serial.println("[Pair] Authentication failed");
      return;
    }
    String mac = String(connInfo.getAddress().toString().c_str()); mac.toLowerCase();
    Serial.printf("[Pair] BONDED with %s\n", mac.c_str());
    Storage::addBondedAddress(mac);
    g_bonded = Storage::loadBondedAddresses();
    g_pairing = false;
  }
};
static PairingServerCallbacks pairCb;

// =========================================================================
//  PUBLIC API
// =========================================================================
void bleProximity_begin() {
  g_bonded = Storage::loadBondedAddresses();
  Serial.printf("[Proximity] %d bonded device(s)\n", (int)g_bonded.size());

  // NimBLE is already initialized by BleKeyboard::begin().
  // Configure for Numeric Comparison capable security.
  NimBLEDevice::setSecurityAuth(true /*bond*/, true /*MITM*/, true /*SC*/);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);
}

static void runScanOnce() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCb, false);
  scan->setActiveScan(false);   // passive = lower power
  scan->setInterval(100);
  scan->setWindow(99);
  scan->start(PROXIMITY_SCAN_TIME_MS / 1000, false);
}

void bleProximity_tick() {
  uint32_t now = millis();

  // Pairing timeout
  if (g_pairing && (now - g_pairStartMs > PAIRING_TIMEOUT_MS)) {
    bleProximity_cancelPairing();
  }

  // Periodic scan (skipped during pairing — radio busy)
  if (!g_pairing && (now - g_lastScanMs > PROXIMITY_INTERVAL_MS)) {
    g_lastScanMs = now;
    runScanOnce();
  }
}

bool bleProximity_phonePresent() {
  if (g_bonded.empty()) {
    // No phone bonded yet — proximity gate disabled
    return true;
  }
  if (g_lastSeenMs == 0) return false;
  if (millis() - g_lastSeenMs > PROXIMITY_GRACE_MS) return false;
  return true;
}

void bleProximity_startPairing() {
  // Generate a random 6-digit code for Numeric Comparison
  g_pairCode = (esp_random() % 900000) + 100000;
  g_pairing = true;
  g_pairStartMs = millis();

  // Reconfigure NimBLE security & start advertising as pair-capable
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&pairCb);
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(BLE_DEVICE_NAME);
  adv->start();
  Serial.printf("[Pair] Started pairing. Code: %u\n", g_pairCode);
}

void bleProximity_cancelPairing() {
  g_pairing = false;
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  if (adv) adv->stop();
  Serial.println("[Pair] Pairing cancelled");
}

bool bleProximity_isPairing() { return g_pairing; }
uint32_t bleProximity_currentPairCode() { return g_pairCode; }

void bleProximity_unbond(const String& macAddr) {
  String m = macAddr; m.toLowerCase();
  Storage::removeBondedAddress(m);
  g_bonded = Storage::loadBondedAddresses();

  // Also remove from NimBLE bond store
  NimBLEAddress addr(m.c_str(), BLE_ADDR_PUBLIC);
  NimBLEDevice::deleteBond(addr);
}

std::vector<String> bleProximity_bondedList() {
  return Storage::loadBondedAddresses();
}
