#include "ride_fakes.h"
#include "ui/components/button.h"
#include "ui/components/label.h"
#include "ui/components/page_header.h"
#include "ui/components/value_tile.h"
#include "ui/components/hero_value.h"
#include "ui/colors.h"
#include <algorithm>
#include <cassert>
#include <string>

uint32_t fakeMillis = 0;
Canvas canvas;

static bool has(const std::string& command) {
  return std::find(canvas.commands.begin(), canvas.commands.end(), command) != canvas.commands.end();
}

int main() {
  using namespace uitheme;
  const auto& t = ui::activeTheme();

  canvas.fillScreen(0);
  ui::drawButton(canvas, {12, 198, 216, 48}, {"Pause", ui::Icon::Pause, true, false}, STYLE_BUTTON_PRIMARY, t);
  assert(canvas.commands.front() == "R|12|198|216|48|8|" + std::to_string(ui::accent));
  assert(canvas.commands.back() == "T|2|60|211|0|Pause");

  canvas.fillScreen(0);
  ui::drawButton(canvas, {12, 198, 216, 48}, {"Pause", ui::Icon::Pause, true, true}, STYLE_BUTTON_PRIMARY, t);
  assert(canvas.commands.front() == "R|12|198|216|48|8|33808");  // 0x8410 while held

  // Icon-only map control: inset fill, centered icon, no text.
  canvas.fillScreen(0);
  ui::drawButton(canvas, {0, 266, 48, 44}, {"", ui::Icon::Minus, true, false}, STYLE_BUTTON_MAP, t);
  assert(canvas.commands.front() == "R|2|266|44|44|8|" + std::to_string(ui::panel));
  assert(canvas.commands.back().rfind("L|", 0) == 0);
  assert(has("L|17|288|30|288|" + std::to_string(ui::accent)));  // Minus cell at x+(w-24)/2=12, y+10

  // Long titles stay inside the 48..192 strip between the two header icons.
  canvas.fillScreen(0);
  ui::drawPageHeader(canvas, "A very long custom page title here", t);
  assert(has("C|48|0|144|44") && has("U"));
  assert(has("T|1|48|12|65535|A very long custom page title here"));

  wmodel::StatusModel s;
  wmodel::modelStatus(true, 2, 140, wmodel::RIDE_HINT_ACTIVE, 250, 0, 7, s);
  assert(!strcmp(s.gps, "GPS 99") && !strcmp(s.battery, "100%") && !strcmp(s.page, "1/7"));
  assert(s.gps_tone == wmodel::TONE_SUCCESS && !strcmp(s.ride, "REC") && s.ride_tone == wmodel::TONE_SUCCESS);
  wmodel::modelStatus(false, 1, 0, wmodel::RIDE_HINT_IDLE, 5, 1, 2, s);
  assert(!strcmp(s.gps, "GPS weak") && s.gps_tone == wmodel::TONE_WARNING);
  assert(!strcmp(s.ride, "IDLE") && s.ride_tone == wmodel::TONE_MUTED && !strcmp(s.page, "2/2"));
  wmodel::modelStatus(false, 0, 0, wmodel::RIDE_HINT_PAUSED, 5, 0, 1, s);
  assert(!strcmp(s.gps, "GPS --") && !strcmp(s.ride, "PAUSE") && s.ride_tone == wmodel::TONE_WARNING);

  canvas.fillScreen(0);
  ui::drawStatusBar(canvas, s, t);
  assert(has("T|1|4|302|" + std::to_string(ui::warning) + "|GPS --"));
  assert(has("T|0|216|307|" + std::to_string(ui::muted) + "|1/1"));

  // Tile: hairline, muted label at +4,+4 and the toned value at +4,+24.
  wmodel::TileModel m;
  wmodel::modelCadence(88, m);
  canvas.fillScreen(0);
  ui::drawValueTile(canvas, {4, 200, 74, 54}, m, true, t);
  assert(has("L|4|200|77|200|" + std::to_string(t.colors[TOK_HAIRLINE])));
  assert(has("T|1|8|204|" + std::to_string(ui::muted) + "|cad"));
  assert(has("T|2|8|224|" + std::to_string(ui::accent) + "|88"));

  wmodel::HeroModel h;
  wmodel::modelSpeed(24.6f, false, wmodel::SPEED_HINT_GPS, h);
  canvas.fillScreen(0);
  ui::drawHeroValue(canvas, {4, 48, 232, 94}, h, true, t);
  assert(has("T|3|8|72|65535|24.6") && has("T|1|192|124|" + std::to_string(ui::muted) + "|km/h"));

  canvas.fillScreen(0);
  ui::drawLabel(canvas, 12, 44, "RECORDING", STYLE_TEXT_MUTED, t, wmodel::TONE_SUCCESS);
  assert(canvas.commands.back() == "T|1|12|44|" + std::to_string(ui::success) + "|RECORDING");
  puts("component tests passed");
}
