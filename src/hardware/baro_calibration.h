#pragma once
#include <cmath>

inline bool baroReferenceValid(float reference) {
  return std::isfinite(reference) && reference >= 800 && reference <= 1200;
}
inline float baroReferenceForElevation(float pressure, int elevation) {
  if (!std::isfinite(pressure) || pressure <= 300 || pressure >= 1100 ||
      elevation < -500 || elevation > 9000) return NAN;
  float reference = pressure / std::pow(1.0f - elevation / 44330.0f, 1.0f / 0.1903f);
  return baroReferenceValid(reference) ? reference : NAN;
}
enum class BaroCalibrationStatus { Ready, Saving, Saved, NoData, RideActive, Failed };
float getBaroReference();
BaroCalibrationStatus getBaroCalibrationStatus();
bool requestBaroCalibration(int elevation);
bool getBaroAutoEnabled();
bool baroAutoDone();
void setBaroAutoEnabled(bool enabled);
bool requestBaroAutoCalibration(int elevation);
