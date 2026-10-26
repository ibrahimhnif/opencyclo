#include "ui/engine/widget_model.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
  using namespace wmodel;

  HeroModel hero;
  modelSpeed(36.0f, false, SPEED_HINT_GPS, hero);
  assert(!strcmp(hero.source, "speed . gps"));
  assert(!strcmp(hero.value, "36.0"));
  assert(!strcmp(hero.unit, "km/h"));

  modelSpeed(36.0f, true, SPEED_HINT_BLE, hero);
  assert(!strcmp(hero.source, "speed . ble"));
  assert(!strcmp(hero.unit, "mph"));  // 36 km/h -> ~22.4 mph
  assert(!strcmp(hero.value, "22.4"));

  TileModel tile;
  modelDistance(12.5f, false, tile);
  assert(!strcmp(tile.label, "dist (km)"));
  assert(!strcmp(tile.value, "12.50"));
  assert(tile.tone == TONE_TEXT);
  modelDistance(12.5f, true, tile);
  assert(!strcmp(tile.label, "dist (mi)"));

  modelRideTime(125, tile);
  assert(!strcmp(tile.value, "02:05"));
  modelRideTime(3725, tile);
  assert(!strcmp(tile.value, "01:02:05"));

  modelCadence(87, tile);
  assert(!strcmp(tile.label, "cad"));
  assert(!strcmp(tile.value, "87"));
  assert(tile.tone == TONE_ACCENT);
  modelCadence(-1, tile);
  assert(!strcmp(tile.value, "0"));

  // Heart rate and power match what the panel has always drawn: "heart"/"0"
  // in danger, "power"/"0" in success.
  modelHeartRate(-1, tile);
  assert(!strcmp(tile.label, "heart") && !strcmp(tile.value, "0") && tile.tone == TONE_DANGER);
  modelPower(212, tile);
  assert(!strcmp(tile.label, "power") && !strcmp(tile.value, "212") && tile.tone == TONE_SUCCESS);
  modelPower(-1, tile);
  assert(!strcmp(tile.value, "0"));

  modelAltitude(100, true, true, tile);
  assert(!strcmp(tile.label, "alt (ft)") && !strcmp(tile.value, "328") && tile.tone == TONE_TEXT);
  modelAltitude(100, true, false, tile);
  assert(!strcmp(tile.label, "alt (m)") && !strcmp(tile.value, "100"));
  modelAltitude(100, false, false, tile);
  assert(!strcmp(tile.value, "--"));
  modelTotalAscent(100, true, tile);
  assert(!strcmp(tile.label, "asc (ft)") && !strcmp(tile.value, "328") && tile.tone == TONE_SUCCESS);
  modelAvgSpeed(21.4f, true, tile);
  assert(!strcmp(tile.label, "avg") && !strcmp(tile.value, "13.3"));
  modelMaxSpeed(48.2f, false, tile);
  assert(!strcmp(tile.label, "max") && !strcmp(tile.value, "48.2"));
  modelGrade(4.0f, tile);
  assert(!strcmp(tile.label, "grade") && !strcmp(tile.value, "+4.0%") && tile.tone == TONE_WARNING);
  modelGrade(-3.0f, tile);
  assert(tile.tone == TONE_ACCENT);
  modelGrade(0.0f, tile);
  assert(tile.tone == TONE_SUCCESS);
  modelBattery(19, tile);
  assert(!strcmp(tile.label, "battery") && !strcmp(tile.value, "19%") && tile.tone == TONE_WARNING);
  modelBattery(20, tile);
  assert(tile.tone == TONE_SUCCESS);

  puts("widget model tests passed");
}
