#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "GPSManager.h"
#include "DHTManager.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

class DisplayManager {
public:
    DisplayManager(int scl, int sda);
    void begin();
    void update(GPSManager& gps, DHTManager& dht);

private:
    Adafruit_SSD1306 _display;
    int _scl;
    int _sda;
};

