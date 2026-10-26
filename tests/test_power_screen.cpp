#include "ride_fakes.h"
#include "ui/power_screen.h"
#include "ui/power_view.h"
#include <algorithm>
#include <cassert>

uint32_t fakeMillis = 0;
Canvas canvas;

static int count(const std::string& command) {
  return int(std::count(canvas.commands.begin(), canvas.commands.end(), command));
}

int main(int argc, char**) {
  auto v = powerui::view(82, 4.01f, false, false, nullptr);
  assert(!strcmp(v.battery, "Battery estimate: 82%  4.01 V"));
  assert(!strcmp(v.usb, "USB charging is automatic."));
  assert(!strcmp(v.charge, "Charge status unavailable.") && !strcmp(v.warning, "Power off / restart ends this ride."));
  assert(!strcmp(v.footer, "Wake after power off: press BOOT"));
  assert(!strcmp(powerui::view(5, 3.3f, true, true, "Saving ride...").footer, "Saving ride..."));
  assert(!strcmp(powerui::view(5, 3.3f, true, true, nullptr).usb, "USB power connected"));
  assert(!strcmp(powerui::view(5, 3.3f, true, false, nullptr).usb, "USB power disconnected"));
  assert(powerui::hit(20, 140) == 0 && powerui::hit(20, 190) == 1 && powerui::hit(20, 240) == 2);
  assert(powerui::hit(20, 300) == -1 && powerui::hit(5, 140) == -1);
  assert(!strcmp(powerui::buttonLabel(1), "Restart") && powerui::buttonIcon(0) == ui::Icon::Power);

  const char* names[] = {"power_idle", "power_restart_pressed", "power_saving"};
  for (int i = 0; i < 3; i++) {
    auto view = i == 2 ? powerui::view(82, 4.01f, true, true, "Saving ride...") : v;
    canvas.fillScreen(0);
    drawPowerScreen(canvas, view, i == 1 ? 1 : -1, ui::activeTheme());
    if (i == 1) {
      assert(count("R|12|182|216|44|8|33808") == 1);  // Restart held
      assert(count("T|0|60|196|65535|Restart") == 1);
      assert(count("T|0|12|14|65535|Power") == 1);
      assert(count("T|0|12|282|65535|Wake after power off: press BOOT") == 1);
    }
    if (argc > 1) {
      printf("SCREEN|%s\n", names[i]);
      for (auto& c : canvas.commands) puts(c.c_str());
    }
  }
  if (argc == 1) puts("power screen tests passed");
}
