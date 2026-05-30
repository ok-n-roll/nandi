#include "WebServerManager.h"
#include <WiFi.h>

WebServerManager::WebServerManager(RelayController &relay, BleLock &bleLock, GPSManager &gps)
  : _relay(relay), _bleLock(bleLock), _gps(gps), _server(80) {}

void WebServerManager::begin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Nandi", "oprst8090");

  _server.on("/", HTTP_GET, [this]() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;} button{width:100%;padding:15px;margin:10px 0;font-size:18px;}</style>";
    html += "</head><body>";
    html += "<h1>Nandi Admin</h1>";
    html += "<a href='/update'><button>Update Firmware</button></a>";
    html += "<form action='/reset-daily' method='POST'><button type='submit'>Reset Odometer</button></form>";
    html += "<form action='/reset' method='POST'><button type='submit'>Reset BLE</button></form>";
    html += "</body></html>";
    _server.send(200, "text/html", html);
  });

  _server.on("/reset", HTTP_POST, [this]() {
    _bleLock.clearOwnerAndBonds();
    _server.send(200, "text/plain", "Reset...");
    delay(1000);
    ESP.restart();
  });

  _server.on("/reset-daily", HTTP_POST, [this]() {
    _gps.resetDailyOdometer();
    _server.send(200, "text/plain", "Reset OK");
  });

  ElegantOTA.onStart([this]() {
    _bleLock.stop();
  });

  ElegantOTA.begin(&_server);
  _server.begin();
}



void WebServerManager::loop() {
  _server.handleClient();
  ElegantOTA.loop();
}

