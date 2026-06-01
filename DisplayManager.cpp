#include "DisplayManager.h"
#include <Wire.h>

DisplayManager::DisplayManager(int scl, int sda)
    : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1), _scl(scl), _sda(sda) {}

void DisplayManager::begin() {
  Wire.begin(_sda, _scl);
  if (!_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // Serial.println(F("SSD1306 allocation failed"));
  }
  _display.clearDisplay();
  _display.setTextColor(WHITE);
  _display.display();
}

void DisplayManager::setPower(bool on) {
    if (on == _isPoweredOn) return;
    
    if (on) {
        _display.ssd1306_command(SSD1306_DISPLAYON);
    } else {
        _display.ssd1306_command(SSD1306_DISPLAYOFF);
    }
    _isPoweredOn = on;
}


void DisplayManager::update(GPSManager& gps, DHTManager& dht) {
  _display.clearDisplay();

  // --- Top Row (Time, Humidity, Temperature) ---
  _display.setTextSize(2);
  _display.setTextColor(WHITE);

  // Time (Left) and Humidity (Next to it)
  _display.setCursor(0, 0);
  _display.print(gps.getTimeString());
  _display.print(" ");

  _display.setTextSize(1);
  if (dht.isDataValid()) {
    _display.print(String((int)dht.getHumidity()));
    _display.print("%");
  } else {
    _display.print("-");
  }

  // Temperature (Strictly Right Aligned)
  if (dht.isDataValid()) {
    String tempStr = String((int)dht.getTemperature());
    int tempWidth = tempStr.length() * 12; // size 2 character width
    int degreeWidth = 6;                   // size 1 character width
    int tempX = SCREEN_WIDTH - (tempWidth + degreeWidth);

    _display.setCursor(tempX, 0);
    _display.setTextSize(2);
    _display.print(tempStr);
    _display.setTextSize(1);
    _display.print((char)247);
  } else {
    String tempStr = "--";
    int tempWidth = tempStr.length() * 12;
    int tempX = SCREEN_WIDTH - tempWidth;
    _display.setCursor(tempX, 0);
    _display.setTextSize(2);
    _display.print(tempStr);
  }

  // --- Speed (Middle) ---
  float currentSpeed = gps.getSpeedKmph();
  int speedInt = 0;
  int speedFrac = 0;

  // Only show speed if GPS has a fix and speed is above a minimum threshold (3 km/h)
  if (gps.isFixed() && currentSpeed >= 3.0) {
    speedInt = floor(currentSpeed);
    speedFrac = round((currentSpeed - speedInt) * 10);
    if (speedFrac >= 10) {
      speedInt++;
      speedFrac = 0;
    }
  }

  String speedIntStr = String(speedInt);
  String speedFracStr = String(speedFrac);

  // --- Animation & Version ---
  unsigned long now = millis();
  if (now - _lastAnimationChange >= 1000) {
    _animationFrame = (_animationFrame % 3) + 1;
    _lastAnimationChange = now;
  }

  _display.setTextSize(1);
  _display.setCursor(0, 20);
  _display.print("v-1");
  _display.setCursor(0, 30);
  for (int i = 0; i < _animationFrame; i++)
    _display.print(".");

  _display.setTextSize(4);
  int16_t x1, y1;
  uint16_t w, h;
  _display.getTextBounds(speedIntStr, 0, 0, &x1, &y1, &w, &h);
  uint16_t intPartWidth = w;
  _display.setTextSize(3);
  _display.getTextBounds(speedFracStr, 0, 0, &x1, &y1, &w, &h);

  _display.setCursor((SCREEN_WIDTH - (intPartWidth + w)) / 2, 20);
  _display.setTextSize(4);
  _display.print(speedIntStr);
  _display.setTextSize(3);
  _display.print(speedFracStr);

  // --- Odometers (Bottom) ---
  auto printOdometer = [this](float val, int x, int y, bool rightAlign) {
    int iPart = floor(val);
    int fPart = round((val - iPart) * 10);
    if (fPart >= 10) {
      iPart++;
      fPart = 0;
    }

    String iStr = String(iPart);
    String fStr = String(fPart);

    this->_display.setTextSize(2);
    int16_t x1, y1;
    uint16_t wInt, hInt;
    this->_display.getTextBounds(iStr, 0, 0, &x1, &y1, &wInt, &hInt);

    this->_display.setTextSize(1);
    uint16_t wFrac, hFrac;
    this->_display.getTextBounds(fStr, 0, 0, &x1, &y1, &wFrac, &hFrac);

    uint16_t totalW = wInt + wFrac;
    int startX = rightAlign ? (SCREEN_WIDTH - totalW - x) : x;

    this->_display.setCursor(startX, y);
    this->_display.setTextSize(2);
    this->_display.print(iStr);
    this->_display.setTextSize(1);
    this->_display.print(fStr);
  };

  printOdometer(gps.getDailyOdometer(), 0, 50, false);
  printOdometer(gps.getTotalOdometer(), 0, 50, true);

  _display.display();
}
