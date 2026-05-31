#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "RelayController.h"
#include "BleLock.h"
#include "GPSManager.h"
#include "Config.h"

class WebServerManager {
public:
  WebServerManager(RelayController &relay, BleLock &bleLock, GPSManager &gps);
  void begin();
  void loop();

private:
  void handleCloudUpdate(); // New helper method

  RelayController &_relay;
  BleLock &_bleLock;
  GPSManager &_gps;
  WebServer _server;

  bool _triggerCloudUpdate = false;
  String _wifiSsid;
  String _wifiPass;
  String _firmwareUrl;
};

