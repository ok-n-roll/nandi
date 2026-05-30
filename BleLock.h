#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <Preferences.h>
#include <esp_gap_ble_api.h>

#include "RelayController.h"

#if !defined(CONFIG_BLUEDROID_ENABLED)
#error "This sketch requires the ESP32 Arduino BLE Bluedroid stack."
#endif

class BleLock {
public:
  BleLock(const char *deviceName, int clearOwnerPin, uint32_t ownerResetVersion, RelayController &relay);

  void begin();
  void loop();
  void clearOwnerAndBonds();

private:
  friend class RelayServerCallbacks;
  friend class RelaySecurityCallbacks;

  void setupBle();
  void startAdvertising();
  void loadOwner();
  void finishOwnerSetup();
  void saveOwnerIdentity(const String &address, const String &irk);
  bool isOwnerAddress(const String &address) const;
  bool isOwnerIdentity(const String &address, const String &irk) const;
  bool learnOwnerFromSingleBond(const String &reason);
  bool saveOwnerFromBondedPeer(const esp_bd_addr_t peerAddress, const String &fallbackAddress, const String &reason);
  String findPeerIrk(const esp_bd_addr_t peerAddress);
  void dumpBondList(const String &reason);
  void removeNonOwnerBonds();
  void removeBond(const esp_bd_addr_t address);

  static String addressToString(const esp_bd_addr_t address);
  static bool addressesEqual(const esp_bd_addr_t left, const esp_bd_addr_t right);
  static String bytesToHexString(const uint8_t *bytes, size_t length);
  static String bondIrkToString(const esp_ble_bond_dev_t &device);

  const char *_deviceName;
  int _clearOwnerPin;
  uint32_t _ownerResetVersion;
  RelayController &_relay;
  Preferences _prefs;

  BLEServer *_bleServer = nullptr;
  BLEHIDDevice *_hidDevice = nullptr;
  BLECharacteristic *_keyboardInput = nullptr;

  String _ownerAddress;
  String _ownerIrk;
  bool _ownerIsSet = false;
  bool _ownerIrkIsSet = false;
  bool _ownerAuthenticated = false;
  bool _bleConnected = false;
  bool _advertisingConfigured = false;
  bool _restartAdvertising = false;
  bool _disconnectPending = false;
  bool _waitingForUnknownAuth = false;
  bool _waitingForFirstOwnerAuth = false;
  bool _relayOffGraceActive = false;
  uint16_t _activeConnId = 0xFFFF;
  esp_bd_addr_t _activePeerBdAddr = {0};
  String _activePeerAddress;
  unsigned long _unknownAuthStartedMs = 0;
  unsigned long _firstOwnerAuthStartedMs = 0;
  unsigned long _relayOffAtMs = 0;
};
