#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

class GPSManager {
public:
    GPSManager(int rxPin, int txPin);
    void begin();
    void loop();
    
    bool isFixed();
    float getSpeedKmph();
    String getTimeString();
    float getDailyOdometer();
    float getTotalOdometer();
    float getServiceOdometer();
    void resetDailyOdometer();
    void resetServiceOdometer();
    double getLat();
    double getLon();

private:
    TinyGPSPlus _gps;
    HardwareSerial _serial;
    float _dailyOdometer = 0.0;
    float _totalOdometer = 0.0;
    float _serviceOdometer = 0.0;
    double _lastLat = 0.0;
    double _lastLon = 0.0;
    unsigned long _lastUpdate = 0;
};
