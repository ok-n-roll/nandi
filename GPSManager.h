#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Preferences.h>

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
    void saveOdometers();
    TinyGPSPlus _gps;
    HardwareSerial _serial;
    Preferences _prefs;
    
    float _dailyOdometer = 0.0;
    float _totalOdometer = 0.0;
    float _serviceOdometer = 0.0;
    float _lastSavedDaily = 0.0;
    float _lastSavedTotal = 0.0;
    float _lastSavedService = 0.0;
    
    double _lastLat = 0.0;
    double _lastLon = 0.0;
    unsigned long _lastUpdate = 0;
    unsigned long _lastSaveTime = 0;
};
