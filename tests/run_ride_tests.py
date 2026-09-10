"""Compile production ride lifecycle and touch UI with fake peripherals."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="opencyclo-ride-tests-") as temp:
    work = Path(temp)
    for header in ["Arduino.h", "freertos/FreeRTOS.h", "freertos/semphr.h", "hardware/display.h"]:
        target = work / header
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "ride_fakes.h"\n')
    binary = work / "ride"
    subprocess.run([
        "c++", "-std=c++11", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
        f"-I{work}", f"-I{root}/tests", f"-I{root}/src",
        str(root / "src/core/telemetry_state.cpp"),
        str(root / "src/ui/ride_menu.cpp"),
        str(root / "tests/test_ride_session.cpp"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True, timeout=30)
