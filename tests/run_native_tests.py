"""Run firmware logic tests with the host C++ compiler; no board required."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix="opencyclo-tests-") as work:
    work = Path(work)
    # Replace only hardware dependencies; compile the production power.cpp.
    for header in (
        "hardware/ble_task.h", "hardware/display.h", "storage/logger_task.h",
        "driver/gpio.h", "driver/rtc_io.h", "esp_sleep.h", "esp_attr.h",
        "hardware/battery.h", "storage/settings.h",
    ):
        target = work / header
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "power_fakes.h"\n')

    for header in ("Arduino.h", "FS.h", "SD_MMC.h", "core/telemetry_state.h"):
        target = work / header
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "gpx_fakes.h"\n')

    for name, sources, flags in (
        ("control_layout", ["tests/test_control_layout.cpp"], []),
        ("button", ["tests/test_power_button.cpp"], []),
        ("power", ["tests/test_power.cpp", "src/hardware/power.cpp",
                   "src/ui/charging_screen.cpp"], []),
        ("power_usb", ["tests/test_power.cpp", "src/hardware/power.cpp",
                       "src/ui/charging_screen.cpp"],
         ["-DPIN_USB_POWER_SENSE=2"]),
        ("gpx", ["tests/test_gpx_writer.cpp", "src/storage/gpx_writer.cpp"], []),
    ):
        binary = work / name
        subprocess.run([
            "c++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
            *flags,
            f"-I{work}", f"-I{ROOT / 'tests'}", f"-I{ROOT / 'src'}",
            *(str(ROOT / source) for source in sources), "-o", str(binary),
        ], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
