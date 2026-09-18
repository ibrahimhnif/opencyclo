"""Exercise production diagnostic writer with fake SD, enabled and compiled out."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="gps-diagnostic-tests-") as temp:
    work = Path(temp)
    for name in ("Arduino.h", "SD_MMC.h", "freertos/FreeRTOS.h", "freertos/semphr.h"):
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "diagnostic_fakes.h"\n')
    for enabled in (0, 1):
        binary = work / f"test-{enabled}"
        subprocess.run(["c++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=undefined", f"-DGPS_DIAGNOSTICS_ENABLED={enabled}",
                        f"-I{work}", f"-I{root / 'tests'}", f"-I{root / 'src'}",
                        str(root / "tests/test_gps_diagnostics.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
