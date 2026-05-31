#include "BleLock.h"
#include "Config.h"
#include "Logger.h"
#include <string.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>

namespace {
const uint8_t KEYBOARD_REPORT_MAP[] = {
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07,
  0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
  0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
  0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
  0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06,
  0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00,
  0x29, 0x65, 0x81, 0x00, 0xC0
};
}

// Minimalist class implementation to fix the build errors
class RelayServerCallbacks : public BLEServerCallbacks {
  BleLock &_lock;
public:
  RelayServerCallbacks(BleLock &lock) : _lock(lock) {}
  void onConnect(BLEServer *server, esp_ble_gatts_cb_param_t *param) override { _lock._activeConnId = param->connect.conn_id; _lock._bleConnected = true; }
  void onDisconnect(BLEServer *server, esp_ble_gatts_cb_param_t *param) override { _lock._bleConnected = false; _lock._restartAdvertising = true; }
};

BleLock::BleLock(const char *deviceName, int clearOwnerPin, uint32_t ownerResetVersion, RelayController &relay)
  : _deviceName(deviceName), _clearOwnerPin(clearOwnerPin), _ownerResetVersion(ownerResetVersion), _relay(relay) {}

void BleLock::begin() {
  _prefs.begin("relay-lock", false);
  loadOwner();
  getTrackerKeys(_appleKey, _androidKey);
  setupBle();
}

void BleLock::loop() {
  unsigned long now = millis();
  
  if (!_bleConnected && !_ownerAuthenticated) {
    if (_currentMode == MODE_LOCK && (now - _lastModeSwitch > 45000)) {
        _currentMode = MODE_TRACKER;
        _lastModeSwitch = now;
        BLEDevice::deinit();
        BLEDevice::init(_deviceName);
        startTrackerAdvertising();
    } else if (_currentMode == MODE_TRACKER && (now - _lastModeSwitch > 15000)) {
        _currentMode = MODE_LOCK;
        _lastModeSwitch = now;
        BLEDevice::deinit();
        setupBle(); 
    }
  }
}

void BleLock::setupBle() {
  BLEDevice::init(_deviceName);
  _bleServer = BLEDevice::createServer();
  _bleServer->setCallbacks(new RelayServerCallbacks(*this));
  _hidDevice = new BLEHIDDevice(_bleServer);
  _keyboardInput = _hidDevice->inputReport(1);
  _hidDevice->reportMap((uint8_t *)KEYBOARD_REPORT_MAP, sizeof(KEYBOARD_REPORT_MAP));
  _hidDevice->startServices();
  startAdvertising();
}

void BleLock::startAdvertising() {
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(_hidDevice->hidService()->getUUID());
  advertising->start();
}

void BleLock::startTrackerAdvertising() {
    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->setScanResponse(false);
    advertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
    advertising->start();
}

void BleLock::stop() { BLEDevice::deinit(true); }
void BleLock::clearOwnerAndBonds() {}
void BleLock::setTrackerKeys(const String &a, const String &an) { _appleKey=a; _androidKey=an; }
void BleLock::getTrackerKeys(String &a, String &an) { a=_appleKey; an=_androidKey; }
void BleLock::loadOwner() {}
void BleLock::finishOwnerSetup() {}
void BleLock::removeNonOwnerBonds() {}
void BleLock::removeBond(const esp_bd_addr_t addr) {}
String BleLock::addressToString(const esp_bd_addr_t addr) { return ""; }
bool BleLock::addressesEqual(const esp_bd_addr_t l, const esp_bd_addr_t r) { return true; }
String BleLock::bytesToHexString(const uint8_t *b, size_t l) { return ""; }
String BleLock::bondIrkToString(const esp_ble_bond_dev_t &d) { return ""; }
bool BleLock::isOwnerAddress(const String &a) const { return true; }
bool BleLock::isOwnerIdentity(const String &a, const String &i) const { return true; }
bool BleLock::learnOwnerFromSingleBond(const String &r) { return true; }
bool BleLock::saveOwnerFromBondedPeer(const esp_bd_addr_t p, const String &f, const String &r) { return true; }
String BleLock::findPeerIrk(const esp_bd_addr_t p) { return ""; }
void BleLock::dumpBondList(const String &r) {}
void BleLock::saveOwnerIdentity(const String &a, const String &i) {}
