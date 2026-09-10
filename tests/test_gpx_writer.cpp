#include "gpx_fakes.h"
#include "storage/gpx_writer.h"
#include "storage/ride_log_session.h"
#include <assert.h>

GpxSerial Serial;
FakeCard SD_MMC;
size_t writeLimit=SIZE_MAX;

int main() {
  GpxWriter writer;
  TelemetryState state;
  state.lat = -6.2;
  state.lon = 106.8;
  assert(writer.openNewRideFile(2026, 8, 21, 12, 0, 0));
  const std::string first = writer.getFilename();
  assert(writer.appendTrackPoint(state, 2026, 8, 21, 12, 0, 1));
  assert(writer.closeRideFile());
  assert(!writer.isOpen());
  SD_MMC.failOpen=false;
  // Finish before the first logger tick still creates a well-formed empty GPX.
  GpxWriter quick;
  assert(finishRideLog(quick,true,true,state)==RIDE_SAVE_OK);
  assert(!quick.isOpen());
  assert(finishRideLog(quick,false,true,state)==RIDE_SAVE_NO_FILE);
  assert(finishRideLog(quick,true,false,state)==RIDE_SAVE_NO_FILE);
  assert(writer.openNewRideFile(0,0,0,0,0,0));
  const std::string retryPath=writer.getFilename();
  writeLimit=12;
  assert(!writer.appendTrackPoint(state,0,0,0,0,0,0));
  writeLimit=SIZE_MAX;
  // Pending partial point is completed before footer is appended.
  assert(finishRideLog(writer,true,true,state)==RIDE_SAVE_OK);
  assert(SD_MMC.files[retryPath].find("<time>")==std::string::npos);
  assert(SD_MMC.files[retryPath].find("</trkpt>")!=std::string::npos);
  assert(writer.openNewRideFile(0,0,0,0,0,0));
  const std::string footerPath=writer.getFilename();
  writeLimit=5;
  assert(finishRideLog(writer,true,true,state)==RIDE_SAVE_ERROR && writer.isOpen());
  writeLimit=0;
  assert(finishRideLog(writer,true,true,state)==RIDE_SAVE_ERROR);
  SD_MMC.failOpen=false;
  writeLimit=SIZE_MAX;
  assert(finishRideLog(writer,true,true,state)==RIDE_SAVE_OK && !writer.isOpen());
  auto xml=SD_MMC.files[footerPath];
  assert(xml.find("</trkseg>")==xml.rfind("</trkseg>"));
  assert(xml.substr(xml.size()-7)=="</gpx>\n");
  SD_MMC.failOpen=true;
  assert(finishRideLog(writer,true,true,state)==RIDE_SAVE_ERROR);
  SD_MMC.failOpen=false;
  const std::string saved = SD_MMC.files[first];
  assert(saved.find("<trkpt lat=\"-6.200000\"") != std::string::npos);
  assert(saved.substr(saved.size() - 7) == "</gpx>\n");
  assert(!writer.closeRideFile());

  // Same timestamp, including after creating a new writer on reboot.
  GpxWriter rebooted;
  for (int i = 1; i <= 3; ++i) {
    assert(rebooted.openNewRideFile(2026, 8, 21, 12, 0, 0));
    assert(std::string(rebooted.getFilename()) ==
           "/rides/20260821_120000_" + std::to_string(i) + ".gpx");
    assert(rebooted.closeRideFile());
    assert(SD_MMC.files[first] == saved);
  }

  // No GPS clock: uptime filenames can also repeat across boots.
  assert(writer.openNewRideFile(0, 0, 0, 0, 0, 0));
  assert(writer.closeRideFile());
  assert(rebooted.openNewRideFile(0, 0, 0, 0, 0, 0));
  assert(std::string(rebooted.getFilename()) == "/rides/ride_42_4.gpx");
  assert(rebooted.closeRideFile());

  SD_MMC.failOpen = true;
  assert(!writer.openNewRideFile(2026, 8, 21, 12, 0, 0));
  assert(!writer.isOpen());
  puts("GPX close and filename collision tests passed");
}
