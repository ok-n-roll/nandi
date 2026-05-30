#include <Arduino.h>

#include "BleLock.h"
#include "Config.h"
#include "RelayController.h"
#include "Logger.h"
#include "WebServerManager.h" // New file

RelayController relay(RELAY_PIN, RELAY_ACTIVE_LEVEL);
BleLock bleLock(BLE_DEVICE_NAME, CLEAR_OWNER_PIN, OWNER_RESET_VERSION, relay);
WebServerManager webServerManager(relay, bleLock);

void setup() {
  Serial.begin(115200);
  delay(200);
  logLine("Boot. Firmware diagnostics enabled.");

  relay.begin();
  bleLock.begin();
  webServerManager.begin();
}

void loop() {
  webServerManager.loop();
  bleLock.loop();
  delay(10);
}
