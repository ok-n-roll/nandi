#pragma once

#include <Arduino.h>

inline void logLine(const __FlashStringHelper* message) {
  Serial.print(F("["));
  Serial.print(millis());
  Serial.print(F("] "));
  Serial.println(message);
}

inline void logLine(const String &message) {
  Serial.print(F("["));
  Serial.print(millis());
  Serial.print(F("] "));
  Serial.println(message);
}
