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

  modelHeartRate(-1, tile);
  assert(!strcmp(tile.value, "--"));
  modelPower(-1, tile);
  assert(!strcmp(tile.value, "--"));

  puts("widget model tests passed");
}
