#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "RelayController.h"
#include "BleLock.h"
#include "GPSManager.h"
#include "DHTManager.h"
#include "Config.h"

class WebServerManager {
public:
  WebServerManager(RelayController &relay, BleLock &bleLock, GPSManager &gps, DHTManager &dht);
  void begin();
  void loop();

private:
  void handleCloudUpdate();

  RelayController &_relay;
  BleLock &_bleLock;
  GPSManager &_gps;
  DHTManager &_dht;
  WebServer _server;



  bool _triggerCloudUpdate = false;
  String _wifiSsid;
  String _wifiPass;
  String _firmwareUrl;
};

