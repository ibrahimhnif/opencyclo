from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="opencyclo-csc-") as temp:
    work = Path(temp)
    for name in ["NimBLEDevice.h", "storage/settings.h", "core/telemetry_state.h"]:
        target = work / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "csc_pairing_fakes.h"\n')
    binary = work / "csc"
    subprocess.run(["c++", "-std=c++11", "-Wall", "-Wextra", "-fsanitize=undefined",
        f"-I{work}", f"-I{root}/tests", f"-I{root}/src",
        str(root/"src/hardware/csc_sensors.cpp"), str(root/"tests/test_csc_pairing.cpp"),
        "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=30)
