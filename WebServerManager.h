#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "RelayController.h"
#include "BleLock.h"
#include "Config.h"

class WebServerManager {
public:
  WebServerManager(RelayController &relay, BleLock &bleLock);
  void begin();
  void loop();

private:
  RelayController &_relay;
  BleLock &_bleLock;
  WebServer _server;
};
