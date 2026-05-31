#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLEAdvertising.h>
#include <Preferences.h>
#include "RelayController.h"

enum BleMode { MODE_LOCK, MODE_TRACKER };

class BleLock {
public:
    BleLock(const char *deviceName, int clearOwnerPin, uint32_t ownerResetVersion, RelayController &relay);
    void begin();
    void loop();
    void stop(); 
    void clearOwnerAndBonds();
    void setTrackerKeys(const String &appleKey, const String &androidKey);
    void getTrackerKeys(String &appleKey, String &androidKey);

    // Make friends with Callbacks so they can access private members
    friend class RelayServerCallbacks;
    friend class RelaySecurityCallbacks;

private:
    void setupBle();
    void startAdvertising();
    void startTrackerAdvertising();
    void loadOwner();
    void finishOwnerSetup();
    void removeNonOwnerBonds();
    void removeBond(const esp_bd_addr_t address);
    static String addressToString(const esp_bd_addr_t address);
    static bool addressesEqual(const esp_bd_addr_t left, const esp_bd_addr_t right);
    static String bytesToHexString(const uint8_t *bytes, size_t length);
    static String bondIrkToString(const esp_ble_bond_dev_t &device);
    
    // Existing members
    bool isOwnerAddress(const String &address) const;
    bool isOwnerIdentity(const String &address, const String &irk) const;
    bool learnOwnerFromSingleBond(const String &reason);
    bool saveOwnerFromBondedPeer(const esp_bd_addr_t peerAddress, const String &fallbackAddress, const String &reason);
    String findPeerIrk(const esp_bd_addr_t peerAddress);
    void dumpBondList(const String &reason);
    void saveOwnerIdentity(const String &address, const String &irk);

    BleMode _currentMode = MODE_LOCK;
    unsigned long _lastModeSwitch = 0;
    
    String _appleKey;
    String _androidKey;
    
    const char *_deviceName;
    int _clearOwnerPin;
    uint32_t _ownerResetVersion;
    RelayController &_relay;
    Preferences _prefs;
    BLEServer *_bleServer = nullptr;
    BLEHIDDevice *_hidDevice = nullptr;
    BLECharacteristic *_keyboardInput = nullptr;
    
    bool _advertisingConfigured = false;
    bool _restartAdvertising = false;
    bool _bleConnected = false;
    bool _ownerIsSet = false;
    bool _ownerIrkIsSet = false;
    bool _ownerAuthenticated = false;
    bool _waitingForUnknownAuth = false;
    bool _waitingForFirstOwnerAuth = false;
    bool _relayOffGraceActive = false;
    bool _disconnectPending = false;
    uint16_t _activeConnId = 0xFFFF;
    esp_bd_addr_t _activePeerBdAddr;
    String _activePeerAddress;
    unsigned long _firstOwnerAuthStartedMs = 0;
    unsigned long _unknownAuthStartedMs = 0;
    unsigned long _relayOffAtMs = 0;
    String _ownerAddress;
    String _ownerIrk;
};
