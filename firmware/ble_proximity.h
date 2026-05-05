/*
 * ble_proximity.h - Background BLE scan for bonded phone presence.
 *
 * Behavior:
 *   - On boot: load list of bonded phone MAC addresses from storage.
 *   - Run periodic BLE scans (background, low duty cycle).
 *   - When a bonded phone is seen with RSSI >= PROXIMITY_RSSI_MIN, mark
 *     proximityOK = true. Stay true for PROXIMITY_GRACE_MS after last sight.
 *   - Pairing flow: temporarily put device into "discoverable + pair" mode,
 *     accept first device that completes Numeric Comparison, persist its MAC.
 *
 * NOTE: this uses the NimBLE-Arduino stack via ESP32-BLE-Keyboard's underlying
 * NimBLE — we share the radio. Scans are ~1 sec every PROXIMITY_INTERVAL_MS.
 */
#pragma once
#include <Arduino.h>
#include <vector>

void bleProximity_begin();
void bleProximity_tick();          // call from loop()
bool bleProximity_phonePresent();  // true if phone seen recently

// Pairing flow (called from admin UI: "pair new phone")
void bleProximity_startPairing();
void bleProximity_cancelPairing();
bool bleProximity_isPairing();

// Get a 6-digit pairing code (only valid during pairing)
uint32_t bleProximity_currentPairCode();

// Forget a bonded device
void bleProximity_unbond(const String& macAddr);

// List of currently bonded devices (read from storage)
std::vector<String> bleProximity_bondedList();
