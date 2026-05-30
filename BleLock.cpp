#include "BleLock.h"

#include "Config.h"
#include "Logger.h"

#include <string.h>

namespace {
const uint8_t KEYBOARD_REPORT_MAP[] = {
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x06,        // Usage (Keyboard)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x01,        //   Report ID (1)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0xE0,        //   Usage Minimum (Keyboard LeftControl)
  0x29, 0xE7,        //   Usage Maximum (Keyboard Right GUI)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x08,        //   Report Count (8)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x08,        //   Report Size (8)
  0x81, 0x01,        //   Input (Const,Array,Abs)
  0x95, 0x05,        //   Report Count (5)
  0x75, 0x01,        //   Report Size (1)
  0x05, 0x08,        //   Usage Page (LEDs)
  0x19, 0x01,        //   Usage Minimum (Num Lock)
  0x29, 0x05,        //   Usage Maximum (Kana)
  0x91, 0x02,        //   Output (Data,Var,Abs)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x03,        //   Report Size (3)
  0x91, 0x01,        //   Output (Const,Array,Abs)
  0x95, 0x06,        //   Report Count (6)
  0x75, 0x08,        //   Report Size (8)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x65,        //   Logical Maximum (101)
  0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
  0x19, 0x00,        //   Usage Minimum (Reserved)
  0x29, 0x65,        //   Usage Maximum (Keyboard Application)
  0x81, 0x00,        //   Input (Data,Array,Abs)
  0xC0               // End Collection
};
}

class RelayServerCallbacks : public BLEServerCallbacks {
public:
  explicit RelayServerCallbacks(BleLock &lock) : _lock(lock) {}

  void onConnect(BLEServer *server, esp_ble_gatts_cb_param_t *param) override {
    _lock._activeConnId = param->connect.conn_id;
    memcpy(_lock._activePeerBdAddr, param->connect.remote_bda, sizeof(esp_bd_addr_t));
    _lock._activePeerAddress = BleLock::addressToString(param->connect.remote_bda);
    _lock._bleConnected = true;
    _lock._ownerAuthenticated = false;
    _lock._waitingForUnknownAuth = false;
    _lock._waitingForFirstOwnerAuth = false;
    if (!_lock._relayOffGraceActive) {
      _lock._relay.set(false);
    }

    String peerAddress = _lock._activePeerAddress;
    String peerIrk = _lock.findPeerIrk(param->connect.remote_bda);
    logLine("BLE connected. connId=" + String(_lock._activeConnId) +
            ", peer=" + peerAddress +
            ", peerIrk=" + (peerIrk.length() > 0 ? "yes" : "no") +
            ", ownerSet=" + (_lock._ownerIsSet ? "yes" : "no") +
            ", ownerIrk=" + (_lock._ownerIrkIsSet ? "yes" : "no"));
    _lock.dumpBondList("onConnect");

    if (server->getConnectedCount() > 1) {
      logLine("Rejecting BLE connection: another connection is already active");
      _lock._disconnectPending = true;
      return;
    }

    if (!_lock._ownerIsSet) {
      _lock._waitingForFirstOwnerAuth = true;
      _lock._firstOwnerAuthStartedMs = millis();
      logLine("No owner saved. Waiting for iPhone pairing/authentication before enabling relay.");
      return;
    }

    _lock.learnOwnerFromSingleBond("before owner check");

    if (_lock.isOwnerIdentity(peerAddress, peerIrk)) {
      if (!_lock._ownerIrkIsSet && peerIrk.length() > 0) {
        _lock.saveOwnerIdentity(_lock._ownerAddress, peerIrk);
      }
      _lock._waitingForUnknownAuth = true;
      _lock._unknownAuthStartedMs = millis();
      logLine("Known owner connected. Waiting for authentication before relay ON.");
      return;
    }

    _lock._waitingForUnknownAuth = true;
    _lock._unknownAuthStartedMs = millis();
    logLine("Unknown BLE device connected. Relay stays OFF; waiting for authentication before reject.");
  }

  void onDisconnect(BLEServer *server, esp_ble_gatts_cb_param_t *param) override {
    bool keepRelayOnTemporarily = _lock._ownerAuthenticated && _lock._relay.isOn();
    _lock._bleConnected = false;
    _lock._ownerAuthenticated = false;
    _lock._waitingForUnknownAuth = false;
    _lock._waitingForFirstOwnerAuth = false;
    _lock._activeConnId = 0xFFFF;
    if (keepRelayOnTemporarily) {
      _lock._relayOffGraceActive = true;
      _lock._relayOffAtMs = millis() + RELAY_OFF_GRACE_MS;
      logLine("BLE disconnected. Relay stays ON for " + String(RELAY_OFF_GRACE_MS / 1000) + " seconds.");
    } else {
      _lock._relayOffGraceActive = false;
      _lock._relay.set(false);
      logLine("BLE disconnected. Relay OFF.");
    }
    _lock.learnOwnerFromSingleBond("onDisconnect");
    _lock.dumpBondList("onDisconnect");
    _lock._activePeerAddress = "";
    _lock._restartAdvertising = true;
  }

private:
  BleLock &_lock;
};

class RelaySecurityCallbacks : public BLESecurityCallbacks {
public:
  explicit RelaySecurityCallbacks(BleLock &lock) : _lock(lock) {}

  uint32_t onPassKeyRequest() override {
    return 0;
  }

  void onPassKeyNotify(uint32_t passKey) override {
    Serial.print("BLE passkey: ");
    Serial.println(passKey);
  }

  bool onSecurityRequest() override {
    return true;
  }

  bool onConfirmPIN(uint32_t pin) override {
    Serial.print("BLE confirm PIN: ");
    Serial.println(pin);
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth) override {
    String peerAddress = BleLock::addressToString(auth.bd_addr);
    String peerIrk = _lock.findPeerIrk(auth.bd_addr);
    logLine("BLE auth complete. peer=" + peerAddress +
            ", success=" + (auth.success ? String("yes") : String("no")) +
            ", failReason=" + String((int)auth.fail_reason) +
            ", authMode=0x" + String(auth.auth_mode, HEX) +
            ", peerIrk=" + (peerIrk.length() > 0 ? "yes" : "no"));
    _lock.dumpBondList("onAuthenticationComplete");

    if (!auth.success) {
      logLine("BLE authentication failed: " + peerAddress);
      _lock._ownerAuthenticated = false;
      _lock._waitingForUnknownAuth = false;
      _lock._waitingForFirstOwnerAuth = false;
      _lock._relayOffGraceActive = false;
      _lock._relay.set(false);
      _lock._disconnectPending = true;
      return;
    }

    logLine("BLE authenticated: " + peerAddress);

    if (!_lock._ownerIsSet) {
      if (!_lock.saveOwnerFromBondedPeer(auth.bd_addr, peerAddress, "first owner auth")) {
        _lock.saveOwnerIdentity(peerAddress, peerIrk);
      }
      _lock._ownerAuthenticated = true;
      _lock._waitingForUnknownAuth = false;
      _lock._waitingForFirstOwnerAuth = false;
      _lock._relayOffGraceActive = false;
      _lock._relay.set(true);
      _lock.removeNonOwnerBonds();
      logLine("First paired phone became owner. Relay ON.");
      return;
    }

    _lock.learnOwnerFromSingleBond("auth complete");

    if (_lock.isOwnerIdentity(peerAddress, peerIrk)) {
      if (!_lock._ownerIrkIsSet && peerIrk.length() > 0) {
        _lock.saveOwnerIdentity(_lock._ownerAddress, peerIrk);
      }
      _lock._ownerAuthenticated = true;
      _lock._waitingForUnknownAuth = false;
      _lock._waitingForFirstOwnerAuth = false;
      _lock._relayOffGraceActive = false;
      _lock._relay.set(true);
      _lock.removeNonOwnerBonds();
      logLine("Owner authenticated. Relay ON.");
      return;
    }

    logLine("Unauthorized BLE device rejected: " + peerAddress);
    _lock._ownerAuthenticated = false;
    _lock._waitingForUnknownAuth = false;
    _lock._waitingForFirstOwnerAuth = false;
    _lock._relayOffGraceActive = false;
    _lock._relay.set(false);
    _lock.removeBond(auth.bd_addr);
    _lock._disconnectPending = true;
  }

private:
  BleLock &_lock;
};

BleLock::BleLock(const char *deviceName, int clearOwnerPin, uint32_t ownerResetVersion, RelayController &relay)
  : _deviceName(deviceName),
    _clearOwnerPin(clearOwnerPin),
    _ownerResetVersion(ownerResetVersion),
    _relay(relay) {}

void BleLock::begin() {
  pinMode(_clearOwnerPin, INPUT_PULLUP);

  _prefs.begin("relay-lock", false);
  loadOwner();
  bool autoClearOwner = _prefs.getUInt("resetVer", 0) != _ownerResetVersion;
  if (autoClearOwner) {
    logLine("New owner reset version detected. Owner and BLE bonds will be cleared once.");
    _prefs.remove("owner");
    _prefs.remove("irk");
    _ownerAddress = "";
    _ownerIrk = "";
    _ownerIsSet = false;
    _ownerIrkIsSet = false;
  }

  setupBle();

  if (autoClearOwner) {
    clearOwnerAndBonds();
    _prefs.putUInt("resetVer", _ownerResetVersion);
  } else if (digitalRead(_clearOwnerPin) == LOW) {
    Serial.println("BOOT/GPIO0 is LOW. Clearing owner in 3 seconds...");
    delay(3000);
    if (digitalRead(_clearOwnerPin) == LOW) {
      clearOwnerAndBonds();
      _prefs.putUInt("resetVer", _ownerResetVersion);
    }
  }

  finishOwnerSetup();
}

void BleLock::loop() {
  if (_disconnectPending && _bleServer != nullptr && _activeConnId != 0xFFFF) {
    _disconnectPending = false;
    logLine("Disconnecting BLE connId=" + String(_activeConnId) +
            ", peer=" + _activePeerAddress);
    _bleServer->disconnect(_activeConnId);
  }

  if (_waitingForUnknownAuth && millis() - _unknownAuthStartedMs > UNKNOWN_AUTH_TIMEOUT_MS) {
    _waitingForUnknownAuth = false;
    logLine("Unknown BLE peer auth timeout. Rejecting peer=" + _activePeerAddress);
    _disconnectPending = true;
  }

  if (_waitingForFirstOwnerAuth && millis() - _firstOwnerAuthStartedMs > FIRST_OWNER_AUTH_TIMEOUT_MS) {
    _waitingForFirstOwnerAuth = false;
    logLine("First owner pairing timeout. Disconnecting peer=" + _activePeerAddress);
    _disconnectPending = true;
  }

  if (_restartAdvertising) {
    _restartAdvertising = false;
    delay(100);
    startAdvertising();
  }

  if (_relayOffGraceActive && millis() >= _relayOffAtMs) {
    _relayOffGraceActive = false;
    _relay.set(false);
    logLine("Relay OFF after BLE disconnect grace timeout.");
  }

  if (!_ownerAuthenticated && !_relayOffGraceActive) {
    _relay.set(false);
  }
}

void BleLock::setupBle() {
  BLEDevice::init(_deviceName);

  BLESecurity *security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(true, false, true);
  security->setForceAuthentication(true);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLEDevice::setSecurityCallbacks(new RelaySecurityCallbacks(*this));

  _bleServer = BLEDevice::createServer();
  _bleServer->setCallbacks(new RelayServerCallbacks(*this));

  _hidDevice = new BLEHIDDevice(_bleServer);
  _keyboardInput = _hidDevice->inputReport(1);
  _hidDevice->outputReport(1);
  _hidDevice->manufacturer();
  _hidDevice->manufacturer("ESP32");
  _hidDevice->pnp(0x02, 0xE502, 0xA111, 0x0210);
  _hidDevice->hidInfo(0x00, 0x02);
  _hidDevice->reportMap((uint8_t *)KEYBOARD_REPORT_MAP, sizeof(KEYBOARD_REPORT_MAP));
  _hidDevice->startServices();
  _hidDevice->setBatteryLevel(100);

  uint8_t emptyKeyboardReport[8] = {0};
  _keyboardInput->setValue(emptyKeyboardReport, sizeof(emptyKeyboardReport));

  startAdvertising();
}

void BleLock::startAdvertising() {
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  if (!_advertisingConfigured) {
    advertising->addServiceUUID(_hidDevice->hidService()->getUUID());
    advertising->setAppearance(HID_KEYBOARD);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);
    _advertisingConfigured = true;
  }
  BLEDevice::startAdvertising();
  Serial.println("BLE advertising started.");
}

void BleLock::loadOwner() {
  _ownerAddress = _prefs.getString("owner", "");
  _ownerIrk = _prefs.getString("irk", "");
  _ownerIsSet = _ownerAddress.length() > 0;
  _ownerIrkIsSet = _ownerIrk.length() > 0;
  logLine("Stored owner: address=" + (_ownerIsSet ? _ownerAddress : String("none")) +
          ", irk=" + (_ownerIrkIsSet ? String("yes") : String("no")));
}

void BleLock::finishOwnerSetup() {
  if (_ownerIsSet) {
    logLine("Saved BLE owner: " + _ownerAddress);
    dumpBondList("setup before cleanup");
    learnOwnerFromSingleBond("setup");
    removeNonOwnerBonds();
    dumpBondList("setup after cleanup");
  } else {
    logLine("No BLE owner saved. First connected phone will become owner.");
    dumpBondList("setup no owner");
  }
}

void BleLock::clearOwnerAndBonds() {
  _prefs.remove("owner");
  _prefs.remove("irk");
  _ownerAddress = "";
  _ownerIrk = "";
  _ownerIsSet = false;
  _ownerIrkIsSet = false;
  _ownerAuthenticated = false;
  _relayOffGraceActive = false;
  _relay.set(false);

  int count = esp_ble_get_bond_device_num();
  if (count <= 0) {
    Serial.println("Owner cleared. No BLE bonds were stored.");
    return;
  }

  esp_ble_bond_dev_t *devices = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * count);
  if (devices == nullptr) {
    Serial.println("Owner cleared. Could not allocate bond list.");
    return;
  }

  int listSize = count;
  if (esp_ble_get_bond_device_list(&listSize, devices) == ESP_OK) {
    for (int i = 0; i < listSize; i++) {
      removeBond(devices[i].bd_addr);
    }
  }
  free(devices);

  Serial.println("Owner and all BLE bonds cleared.");
}

void BleLock::saveOwnerIdentity(const String &address, const String &irk) {
  _ownerAddress = address;
  _ownerIrk = irk;
  _ownerIsSet = true;
  _ownerIrkIsSet = _ownerIrk.length() > 0;
  _prefs.putString("owner", _ownerAddress);
  _prefs.putString("irk", _ownerIrk);
  logLine("BLE owner saved. address=" + _ownerAddress + ", irk=" + (_ownerIrkIsSet ? "yes" : "no"));
}

bool BleLock::isOwnerAddress(const String &address) const {
  return _ownerIsSet && address.equalsIgnoreCase(_ownerAddress);
}

bool BleLock::isOwnerIdentity(const String &address, const String &irk) const {
  if (_ownerIrkIsSet && irk.length() > 0 && irk.equalsIgnoreCase(_ownerIrk)) {
    return true;
  }

  return isOwnerAddress(address);
}

bool BleLock::learnOwnerFromSingleBond(const String &reason) {
  if (!_ownerIsSet || _ownerIrkIsSet) {
    return _ownerIrkIsSet;
  }

  int count = esp_ble_get_bond_device_num();
  if (count != 1) {
    logLine("Owner IRK not learned (" + reason + "): bond count=" + String(count));
    return false;
  }

  esp_ble_bond_dev_t device;
  int listSize = 1;
  if (esp_ble_get_bond_device_list(&listSize, &device) != ESP_OK || listSize != 1) {
    logLine("Owner IRK not learned (" + reason + "): bond list read failed");
    return false;
  }

  String bondedAddress = addressToString(device.bd_addr);
  String identityAddress = addressToString(device.bond_key.pid_key.static_addr);
  String bondedIrk = bondIrkToString(device);
  bool bondMatchesSavedOwner = isOwnerAddress(bondedAddress) ||
                               isOwnerAddress(identityAddress) ||
                               (_activePeerAddress.length() > 0 && isOwnerAddress(_activePeerAddress));
  if (!bondMatchesSavedOwner) {
    logLine("Owner IRK not learned (" + reason + "): the only bond does not match saved owner");
    return false;
  }

  saveOwnerIdentity(bondedAddress, bondedIrk);
  logLine("Owner updated from the only BLE bond (" + reason + ")");
  return bondedIrk.length() > 0;
}

bool BleLock::saveOwnerFromBondedPeer(const esp_bd_addr_t peerAddress, const String &fallbackAddress,
                                      const String &reason) {
  int count = esp_ble_get_bond_device_num();
  if (count <= 0) {
    logLine("Owner bond not found (" + reason + "): no stored bonds");
    return false;
  }

  esp_ble_bond_dev_t *devices = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * count);
  if (devices == nullptr) {
    logLine("Owner bond not found (" + reason + "): malloc");
    return false;
  }

  bool saved = false;
  int listSize = count;
  if (esp_ble_get_bond_device_list(&listSize, devices) == ESP_OK) {
    for (int i = 0; i < listSize; i++) {
      bool matchesPeer = addressesEqual(devices[i].bd_addr, peerAddress) ||
                         addressesEqual(devices[i].bond_key.pid_key.static_addr, peerAddress);
      if (matchesPeer || listSize == 1) {
        String bondedAddress = addressToString(devices[i].bd_addr);
        String bondedIrk = bondIrkToString(devices[i]);
        saveOwnerIdentity(bondedAddress.length() > 0 ? bondedAddress : fallbackAddress, bondedIrk);
        logLine("Owner saved from BLE bond (" + reason + ")");
        saved = true;
        break;
      }
    }
  }

  free(devices);
  return saved;
}

String BleLock::findPeerIrk(const esp_bd_addr_t peerAddress) {
  int count = esp_ble_get_bond_device_num();
  if (count <= 0) {
    return "";
  }

  esp_ble_bond_dev_t *devices = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * count);
  if (devices == nullptr) {
    return "";
  }

  String irk;
  int listSize = count;
  if (esp_ble_get_bond_device_list(&listSize, devices) == ESP_OK) {
    for (int i = 0; i < listSize; i++) {
      if (addressesEqual(devices[i].bd_addr, peerAddress) ||
          addressesEqual(devices[i].bond_key.pid_key.static_addr, peerAddress)) {
        irk = bondIrkToString(devices[i]);
        break;
      }
    }
  }

  free(devices);
  return irk;
}

void BleLock::dumpBondList(const String &reason) {
  int count = esp_ble_get_bond_device_num();
  logLine("Bond dump (" + reason + "): count=" + String(count));
  if (count <= 0) {
    return;
  }

  esp_ble_bond_dev_t *devices = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * count);
  if (devices == nullptr) {
    logLine("Bond dump failed: malloc");
    return;
  }

  int listSize = count;
  if (esp_ble_get_bond_device_list(&listSize, devices) == ESP_OK) {
    for (int i = 0; i < listSize; i++) {
      String bondedAddress = addressToString(devices[i].bd_addr);
      String identityAddress = addressToString(devices[i].bond_key.pid_key.static_addr);
      String bondedIrk = bondIrkToString(devices[i]);
      bool isOwnerBond = isOwnerIdentity(bondedAddress, bondedIrk) || isOwnerAddress(identityAddress);
      logLine("  bond[" + String(i) + "] addr=" + bondedAddress +
              " identity=" + identityAddress +
              " keyMask=0x" + String(devices[i].bond_key.key_mask, HEX) +
              " irk=" + (bondedIrk.length() > 0 ? "yes" : "no") +
              " owner=" + (isOwnerBond ? "yes" : "no"));
    }
  } else {
    logLine("Bond dump failed: esp_ble_get_bond_device_list");
  }

  free(devices);
}

void BleLock::removeNonOwnerBonds() {
  int count = esp_ble_get_bond_device_num();
  if (count <= 0) {
    return;
  }

  esp_ble_bond_dev_t *devices = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * count);
  if (devices == nullptr) {
    return;
  }

  int listSize = count;
  if (esp_ble_get_bond_device_list(&listSize, devices) == ESP_OK) {
    for (int i = 0; i < listSize; i++) {
      String bondedAddress = addressToString(devices[i].bd_addr);
      String identityAddress = addressToString(devices[i].bond_key.pid_key.static_addr);
      String bondedIrk = bondIrkToString(devices[i]);
      bool isOwnerBond = isOwnerIdentity(bondedAddress, bondedIrk) || isOwnerAddress(identityAddress);
      if (!isOwnerBond) {
        Serial.print("Removing non-owner BLE bond: ");
        Serial.println(bondedAddress);
        removeBond(devices[i].bd_addr);
      }
    }
  }

  free(devices);
}

void BleLock::removeBond(const esp_bd_addr_t address) {
  esp_bd_addr_t copy;
  memcpy(copy, address, sizeof(esp_bd_addr_t));
  esp_ble_remove_bond_device(copy);
}

String BleLock::addressToString(const esp_bd_addr_t address) {
  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
           address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(buffer);
}

bool BleLock::addressesEqual(const esp_bd_addr_t left, const esp_bd_addr_t right) {
  return memcmp(left, right, sizeof(esp_bd_addr_t)) == 0;
}

String BleLock::bytesToHexString(const uint8_t *bytes, size_t length) {
  String value;
  value.reserve(length * 2);
  for (size_t i = 0; i < length; i++) {
    char part[3];
    snprintf(part, sizeof(part), "%02X", bytes[i]);
    value += part;
  }
  return value;
}

String BleLock::bondIrkToString(const esp_ble_bond_dev_t &device) {
  if ((device.bond_key.key_mask & ESP_LE_KEY_PID) == 0) {
    return "";
  }

  bool hasNonZeroByte = false;
  for (int i = 0; i < 16; i++) {
    if (device.bond_key.pid_key.irk[i] != 0) {
      hasNonZeroByte = true;
      break;
    }
  }

  if (!hasNonZeroByte) {
    return "";
  }

  return bytesToHexString(device.bond_key.pid_key.irk, 16);
}
