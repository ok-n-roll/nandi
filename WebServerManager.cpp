#include "WebServerManager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

WebServerManager::WebServerManager(RelayController &relay, BleLock &bleLock, GPSManager &gps)
    : _relay(relay), _bleLock(bleLock), _gps(gps), _server(80) {}

void WebServerManager::begin() {
  // Load saved OTA & WiFi settings
  Preferences otaPrefs;
  otaPrefs.begin("ota-settings", true);
  _wifiSsid = otaPrefs.getString("ssid", "");
  _wifiPass = otaPrefs.getString("pass", "");
  _firmwareUrl = otaPrefs.getString("url", "");
  otaPrefs.end();

  WiFi.mode(WIFI_AP);
  WiFi.softAP("Nandi", "oprst8090");

  _server.on("/", HTTP_GET, [this]() {
    String appleKey, androidKey;
    _bleLock.getTrackerKeys(appleKey, androidKey);
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Nandi</title>";
    html += "<style>";
    html += "body { font-family: -apple-system, sans-serif; background-color: #121212; color: #e0e0e0; padding: 15px; margin: 0; }";
    html += ".card { background: #1e1e1e; padding: 20px; border-radius: 12px; margin-bottom: 15px; box-shadow: 0 4px 10px rgba(0,0,0,0.3); }";
    html += "h2 { color: #ff9800; font-size: 18px; margin-top: 0; }";
    html += "input { width: 100%; padding: 10px; margin: 8px 0; background: #2a2a2a; border: 1px solid #444; border-radius: 6px; color: #fff; box-sizing: border-box; }";
    html += "button { width: 100%; padding: 12px; background-color: #4caf50; color: white; border: none; border-radius: 6px; font-weight: bold; cursor: pointer; font-size: 15px; }";
    html += "button.btn-blue { background-color: #2196f3; }";
    html += "button.btn-red { background-color: #f44336; }";
    html += "</style></head><body>";
    
    html += "<h1>Nandi</h1>";
    
    // 1. Tracker Keys Card
    html += "<div class='card'>";
    html += "<h2>Tracker Settings</h2>";
    html += "<form action='/save-keys' method='POST'>";
    html += "Apple Find My Key:<input name='appleKey' value='" + appleKey + "' placeholder='Base64 Key'>";
    html += "Android Key:<input name='androidKey' value='" + androidKey + "'>";
    html += "<button type='submit'>Save Keys</button></form>";
    html += "</div>";

    // 2. Cloud OTA Update Card
    html += "<div class='card'>";
    html += "<h2>Cloud Firmware Update (GitHub)</h2>";
    html += "<form action='/save-ota' method='POST'>";
    html += "Home WiFi SSID:<input name='ssid' value='" + _wifiSsid + "'>";
    html += "Home WiFi Password:<input type='password' name='pass' value='" + _wifiPass + "'>";
    html += "Firmware URL (.bin):<input name='url' value='" + _firmwareUrl + "' placeholder='https://...'>";
    html += "<button type='submit' class='btn-blue' style='margin-bottom: 10px;'>Save WiFi & URL</button></form>";
    html += "<form action='/start-cloud-update' method='POST'><button type='submit'>Start Cloud Update</button></form>";
    html += "</div>";

    // 3. Maintenance Card
    html += "<div class='card'>";
    html += "<h2>Maintenance</h2>";
    html += "<form action='/reset-daily' method='POST'><button type='submit' class='btn-blue' style='margin-bottom: 10px;'>Reset Daily Odometer</button></form>";
    html += "<form action='/reset' method='POST'><button type='submit' class='btn-red'>Reset Owner (Clear BLE)</button></form>";
    html += "</div>";
    
    html += "</body></html>";
    
    _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    _server.sendHeader("Expires", "-1");
    _server.send(200, "text/html", html);
  });

  _server.on("/save-keys", HTTP_POST, [this]() {
    String appleKey = _server.arg("appleKey");
    String androidKey = _server.arg("androidKey");
    _bleLock.setTrackerKeys(appleKey, androidKey);
    _server.send(200, "text/plain", "Keys saved. Rebooting to apply...");
    delay(1000);
    ESP.restart();
  });

  _server.on("/save-ota", HTTP_POST, [this]() {
    _wifiSsid = _server.arg("ssid");
    _wifiPass = _server.arg("pass");
    _firmwareUrl = _server.arg("url");

    Preferences otaPrefs;
    otaPrefs.begin("ota-settings", false);
    otaPrefs.putString("ssid", _wifiSsid);
    otaPrefs.putString("pass", _wifiPass);
    otaPrefs.putString("url", _firmwareUrl);
    otaPrefs.end();

    _server.sendHeader("Location", "/");
    _server.send(303);
  });

  _server.on("/start-cloud-update", HTTP_POST, [this]() {
    _server.send(200, "text/html", "<html><body><h2>Connecting to WiFi and downloading firmware...</h2><p>Please wait 1-2 minutes. The device will reboot on success.</p></body></html>");
    _triggerCloudUpdate = true;
  });

  _server.on("/reset", HTTP_POST, [this]() {
    _bleLock.clearOwnerAndBonds();
    _server.send(200, "text/plain", "Owner reset. Rebooting...");
    delay(1000);
    ESP.restart();
  });

  _server.on("/reset-daily", HTTP_POST, [this]() {
    _gps.resetDailyOdometer();
    _server.sendHeader("Location", "/");
    _server.send(303);
  });

  ElegantOTA.onStart([this]() { _bleLock.stop(); });

  ElegantOTA.begin(&_server);
  _server.begin();
  Serial.println("Web server started.");
}

void WebServerManager::loop() {
  _server.handleClient();
  ElegantOTA.loop();

  if (_triggerCloudUpdate) {
    _triggerCloudUpdate = false;
    handleCloudUpdate();
  }
}

void WebServerManager::handleCloudUpdate() {
  Serial.println("Starting HTTP Cloud Update...");
  
  // 1. Stop BLE completely to free heap memory
  _bleLock.stop();
  delay(500);

  // 2. Connect to home WiFi
  Serial.print("Connecting to Home WiFi: ");
  Serial.println(_wifiSsid);
  
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA); // Keep AP active, but act as client too
  WiFi.begin(_wifiSsid.c_str(), _wifiPass.c_str());

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(1000);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to Home WiFi. Reverting to normal AP mode.");
    WiFi.mode(WIFI_AP);
    return;
  }

  Serial.println("\nWiFi Connected! Downloading update from URL...");
  Serial.println(_firmwareUrl);

  // 3. Launch secure HTTPS client for GitHub (bypassing SSL verification for convenience and simplicity)
  WiFiClientSecure client;
  client.setInsecure(); // No certificate verification

  // 4. Trigger HTTP Update
  httpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return ret = httpUpdate.update(client, _firmwareUrl);

  if (ret == HTTP_UPDATE_FAILED) {
    Serial.printf("HTTP Update failed: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    // Revert to normal state if failed
    WiFi.mode(WIFI_AP);
  }
}
