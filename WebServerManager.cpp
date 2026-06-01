#include "WebServerManager.h"
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

WebServerManager::WebServerManager(RelayController &relay, BleLock &bleLock,
                                   GPSManager &gps, DHTManager &dht)
    : _relay(relay), _bleLock(bleLock), _gps(gps), _dht(dht), _server(80) {}

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
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta "
                  "name='viewport' content='width=device-width, "
                  "initial-scale=1.0'><title>Nandi</title>";
    html += "<style>body{font-family:sans-serif;background:#121212;color:#fff;"
            "padding:15px;} "
            ".card{background:#1e1e1e;padding:15px;border-radius:10px;margin-"
            "bottom:10px;} h2{color:#ff9800;margin:0 0 10px 0;} "
            "input{width:100%;padding:8px;margin:5px "
            "0;background:#333;color:#fff;border:none;border-radius:5px;} "
            "button{width:100%;padding:10px;margin:5px "
            "0;border:none;border-radius:5px;font-weight:bold;} "
            ".btn-blue{background:#2196f3;color:#fff;} "
            ".btn-red{background:#f44336;color:#fff;}</style></head><body>";
    html += "<h1>Nandi Admin</h1>";

    // Status
    html += "<div class='card'>";
    html += "<p>Temp: " + String(_dht.getTemperature(), 1) +
            "°C | Hum: " + String(_dht.getHumidity(), 1) + "%</p>";
    html += "<p>Daily: " + String(_gps.getDailyOdometer(), 1) + " km</p>";
    html +=
        "<p>Общий пробег: " + String(_gps.getTotalOdometer(), 1) + " km</p>";
    html += "<p><a href='https://www.google.com/maps/search/?api=1&query=" +
            String(_gps.getLat(), 6) + "," + String(_gps.getLon(), 6) +
            "' target='_blank'>View on Map</a></p>";
    html += "</div>";

    html += "<div class='card'>";
    html +=
        "<p>Суточный пробег: " + String(_gps.getDailyOdometer(), 1) + " км</p>";
    html += "<form action='/reset-daily' method='POST'><button type='submit' "
            "class='btn-blue' style='margin-bottom: 10px;'>Сбросить суточный "
            "пробег</button></form>";
    html += "<p>Сервисный пробег: " + String(_gps.getServiceOdometer(), 1) +
            " км</p>";
    html += "<p><small>Для Yamaha FZ рекомендуется каждые 2000 - 3000 "
            "км</small></p>";
    html += "<form action='/reset-service' method='POST'><button type='submit' "
            "class='btn-blue' style='margin-bottom: 10px;'>Сбросить сервисный "
            "пробег</button></form>";
    html += "</div>";

    // Cloud OTA Update Card
    html += "<div class='card'><h2>Обновление прошивки</h2>";
    html += "<form action='/save-ota' method='POST'>";
    html += "WiFi SSID:<input name='ssid' value='" + _wifiSsid + "'>";
    html += "WiFi Пароль:<input type='password' name='pass' value='" +
            _wifiPass + "'>";
    html +=
        "Firmware URL (.bin):<input name='url' value='" + _firmwareUrl + "'>";
    html += "<button type='submit' class='btn-blue'>Сохранить WiFi и "
            "URL</button></form>";
    html += "<form action='/start-cloud-update' method='POST'><button "
            "type='submit'>Начать обновление</button></form>";
    html += "</div>";

    html += "<div class='card'>";
    html += "<form action='/reset' method='POST'><button type='submit' "
            "class='btn-red'>Сбросить владельца</button></form>";
    html += "</div>";

    html += "</body></html>";
    _server.send(200, "text/html", html);
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
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' "
            "content='width=device-width, initial-scale=1.0'>";
    html += "<title>Updating...</title>";
    html += "<style>body { font-family: sans-serif; background: #121212; "
            "color: #fff; padding: 20px; text-align: center; }</style>";
    html += "<script>setTimeout(() => location.href='/update-status', "
            "2000);</script>";
    html += "</head><body><h2>Starting update...</h2><p>Please "
            "wait...</p></body></html>";
    _server.send(200, "text/html", html);
    _triggerCloudUpdate = true;
  });

  _server.on("/update-status", HTTP_GET, [this]() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' "
            "content='width=device-width, initial-scale=1.0'>";
    html += "<title>Status</title>";
    html += "<style>body { font-family: sans-serif; background: #121212; "
            "color: #fff; padding: 20px; }</style>";
    html += "</head><body><h2>Update in progress</h2>";
    html += "<p>Check device logs or wait for reboot.</p>";
    html += "<a href='/'>Back to Home</a></body></html>";
    _server.send(200, "text/html", html);
  });

  _server.on("/reset", HTTP_POST, [this]() {
    _server.sendHeader("Location", "/");
    _server.send(303);
    delay(500);
    _bleLock.clearOwnerAndBonds();
    delay(500);
    ESP.restart();
  });

  _server.on("/reset-daily", HTTP_POST, [this]() {
    _gps.resetDailyOdometer();
    _server.sendHeader("Location", "/");
    _server.send(303);
  });

  _server.on("/reset-service", HTTP_POST, [this]() {
    _gps.resetServiceOdometer();
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
    Serial.println(
        "\nFailed to connect to Home WiFi. Reverting to normal AP mode.");
    WiFi.mode(WIFI_AP);
    return;
  }

  Serial.println("\nWiFi Connected! Downloading update from URL...");
  Serial.println(_firmwareUrl);

  // 3. Launch secure HTTPS client for GitHub (bypassing SSL verification for
  // convenience and simplicity)
  WiFiClientSecure client;
  client.setInsecure(); // No certificate verification

  // 4. Trigger HTTP Update
  httpUpdate.rebootOnUpdate(true);

  // Добавляем логирование прогресса для отладки
  httpUpdate.onProgress([](int cur, int total) {
    Serial.printf("Update progress: %d%%\n", (cur * 100) / total);
  });

  t_httpUpdate_return ret = httpUpdate.update(client, _firmwareUrl);

  if (ret == HTTP_UPDATE_FAILED) {
    Serial.printf("HTTP Update failed: (%d): %s\n", httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
    // Revert to normal state if failed
    WiFi.mode(WIFI_AP);
  }
}
