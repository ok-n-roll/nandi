#include "GPSManager.h"

GPSManager::GPSManager(int rxPin, int txPin) : _serial(2) { // UART2
    // On ESP32, we need to configure pins
    _serial.begin(9600, SERIAL_8N1, rxPin, txPin);
}

void GPSManager::begin() {
    // Already started in constructor, but can add extra initialization if needed
}

void GPSManager::loop() {
    while (_serial.available() > 0) {
        if (_gps.encode(_serial.read())) {
            if (_gps.location.isValid()) {
                double lat = _gps.location.lat();
                double lon = _gps.location.lng();
                
                // Only count distance if speed is above threshold (e.g., 3.0 km/h) to filter out GPS drift
                bool isMoving = _gps.speed.isValid() && _gps.speed.kmph() > 3.0;

                if (_lastLat != 0.0 && _lastLon != 0.0) {
                    double dLat = radians(lat - _lastLat);
                    double dLon = radians(lon - _lastLon);
                    double a = sin(dLat / 2) * sin(dLat / 2) +
                               cos(radians(_lastLat)) * cos(radians(lat)) *
                               sin(dLon / 2) * sin(dLon / 2);
                    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
                    double dist = 6371.0 * c; // distance in km
                    
                    if (isMoving && dist < 0.5) { // Sanity check: individual update shouldn't exceed 500m (e.g. signal jumps)
                        _dailyOdometer += dist;
                        _totalOdometer += dist;
                        _serviceOdometer += dist;
                    }
                }
                
                // Always update the last position to current to avoid huge distance jumps on speed pick-up
                _lastLat = lat;
                _lastLon = lon;
            }
        }
    }
}


bool GPSManager::isFixed() {
    return _gps.location.isValid();
}

float GPSManager::getSpeedKmph() {
    return _gps.speed.kmph();
}

String GPSManager::getTimeString() {
    if (_gps.time.isValid()) {
        char buffer[10];
        int localHour = (_gps.time.hour() + 7) % 24; // Convert UTC to Vietnam Local Time (UTC+7)
        snprintf(buffer, sizeof(buffer), "%02d:%02d", localHour, _gps.time.minute());
        return String(buffer);
    }
    return "--:--";
}

float GPSManager::getDailyOdometer() { return _dailyOdometer; }
float GPSManager::getTotalOdometer() { return _totalOdometer; }
float GPSManager::getServiceOdometer() { return _serviceOdometer; }
double GPSManager::getLat() { return _lastLat; }
double GPSManager::getLon() { return _lastLon; }
void GPSManager::resetDailyOdometer() { _dailyOdometer = 0.0; }
void GPSManager::resetServiceOdometer() { _serviceOdometer = 0.0; }
