#include <Arduino.h>

#include "BleLock.h"
#include "Config.h"
#include "RelayController.h"
#include "Logger.h"
#include "WebServerManager.h" // New file
#include "GPSManager.h"
#include "DHTManager.h"
#include "DisplayManager.h"

RelayController relay(RELAY_PIN, RELAY_ACTIVE_LEVEL);
BleLock bleLock(BLE_DEVICE_NAME, CLEAR_OWNER_PIN, OWNER_RESET_VERSION, relay);
GPSManager gps(GPS_RX_PIN, GPS_TX_PIN);
DHTManager dht(DHT_PIN);
WebServerManager webServerManager(relay, bleLock, gps, dht);
DisplayManager display(DISPLAY_SCL_PIN, DISPLAY_SDA_PIN);

unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); // Даем время для стабилизации питания и очистки буфера Serial
  Serial.println();
  Serial.println();
  Serial.println("======================================");
  Serial.println("      Nandi System Booting...         ");
  Serial.println("======================================");

  relay.begin();
  bleLock.begin();
  gps.begin();
  dht.begin();
  display.begin();
  webServerManager.begin();
}

void loop() {
  webServerManager.loop();
  bleLock.loop();
  gps.loop();
  dht.loop();
  
  bool active = bleLock.isUnlocked();
  display.setPower(active);
  
  if (active && (millis() - lastDisplayUpdate >= 500)) {
    display.update(gps, dht);
    lastDisplayUpdate = millis();
  }
}
