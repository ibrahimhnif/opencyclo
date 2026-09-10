"""Compile the actual navigation renderer/transfer service with fake peripherals."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='opencyclo-nav-tests-') as temp:
    work=Path(temp)
    for header in ['Arduino.h','NimBLEDevice.h','SD_MMC.h','storage/sd_access.h','core/telemetry_state.h','hardware/display.h']:
        target=work/header;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text('#include "navigation_fakes.h"\n')
    binary=work/'navigation'
    subprocess.run(['c++','-std=c++14','-DUNIT_TEST','-Wall','-Wextra','-fsanitize=undefined','-g',f'-I{work}',f'-I{root}/tests',f'-I{root}/src',str(root/'src/navigation/navigation.cpp'),str(root/'src/navigation/map_renderer.cpp'),str(root/'tests/test_navigation_integration.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=60)
    subprocess.run([str(binary),'memory-failure'],check=True,timeout=60)
