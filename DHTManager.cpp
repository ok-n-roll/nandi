#include "DHTManager.h"

DHTManager::DHTManager(int pin) : _dht(pin, DHT11) {}

void DHTManager::begin() {
    _dht.begin();
}

void DHTManager::loop() {
    if (millis() - _lastUpdate > 2000) {
        float t = _dht.readTemperature();
        float h = _dht.readHumidity();
        if (!isnan(t) && !isnan(h)) {
            _temp = t;
            _hum = h;
            _valid = true;
        }
        _lastUpdate = millis();
    }
}


float DHTManager::getTemperature() { return _temp; }
float DHTManager::getHumidity() { return _hum; }
bool DHTManager::isDataValid() { return _valid; }
