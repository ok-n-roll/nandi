#include "WebServerManager.h"
#include <WiFi.h>

WebServerManager::WebServerManager(RelayController &relay, BleLock &bleLock)
  : _relay(relay), _bleLock(bleLock), _server(80) {}

void WebServerManager::begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Nandi", "oprst8090");

  _server.on("/", HTTP_GET, [this]() {
    String html = "<html><body><h1>Nandi Admin</h1>";
    html += "<p><a href='/update'><button>Update Firmware</button></a></p>";
    html += "<form action='/reset' method='POST'><button type='submit'>Reset Owner (Clear BLE)</button></form>";
    html += "</body></html>";
    _server.send(200, "text/html", html);
  });

  _server.on("/reset", HTTP_POST, [this]() {
    _bleLock.clearOwnerAndBonds();
    _server.send(200, "text/plain", "Owner reset. Rebooting...");
    delay(1000);
    ESP.restart();
  });

  ElegantOTA.begin(&_server);
  _server.begin();
  Serial.println("Web server started.");
}

void WebServerManager::loop() {
  _server.handleClient();
}
