#include "RelayController.h"

#include "Logger.h"

RelayController::RelayController(int pin, int activeLevel)
  : _pin(pin), _activeLevel(activeLevel) {}

void RelayController::begin() {
  pinMode(_pin, OUTPUT);
  set(false);
}

void RelayController::set(bool on) {
  if (_isOn == on) {
    digitalWrite(_pin, on ? _activeLevel : !_activeLevel);
    return;
  }

  _isOn = on;
  digitalWrite(_pin, on ? _activeLevel : !_activeLevel);
  logLine(String("Relay ") + (on ? "ON" : "OFF"));
}

bool RelayController::isOn() const {
  return _isOn;
}
