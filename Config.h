#pragma once

#include <Arduino.h>

const int RELAY_PIN = 4;
const int CLEAR_OWNER_PIN =
    0; // Hold BOOT/GPIO0 LOW while reset to forget the paired iPhone.

const char *const OTA_HOSTNAME = "Nandi";
const char *const OTA_PASSWORD = "oprst8090";

const char *const BLE_DEVICE_NAME = "Nandi";

// Most relay modules are active HIGH. Change this to LOW if your relay is
// active LOW.
const int RELAY_ACTIVE_LEVEL = HIGH;
const uint32_t OWNER_RESET_VERSION = 3;
const unsigned long UNKNOWN_AUTH_TIMEOUT_MS = 30000;
const unsigned long FIRST_OWNER_AUTH_TIMEOUT_MS = 30000;
const unsigned long RELAY_OFF_GRACE_MS = 10000;

// Hardware pins
const int DHT_PIN = 33;
const int DISPLAY_SCL_PIN = 26;
const int DISPLAY_SDA_PIN = 25;
// GPS pins (assumes UART2)
const int GPS_RX_PIN = 16; // RX2
const int GPS_TX_PIN = 17; // TX2

