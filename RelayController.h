#pragma once

#include <Arduino.h>

class RelayController {
public:
  RelayController(int pin, int activeLevel);

  void begin();
  void set(bool on);
  bool isOn() const;

private:
  int _pin;
  int _activeLevel;
  bool _isOn = false;
};
