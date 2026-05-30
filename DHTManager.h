#pragma once
#include <DHT.h>

class DHTManager {
public:
    DHTManager(int pin);
    void begin();
    void loop();
    float getTemperature();
    float getHumidity();
    bool isDataValid();

private:
    DHT _dht;
    float _temp = 0.0;
    float _hum = 0.0;
    bool _valid = false;
    unsigned long _lastUpdate = 0;
};
