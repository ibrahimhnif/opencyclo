"""Exercise production SD cache and injector without touching a real SD card."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='gps-cache-tests-') as temp:
    work = Path(temp)
    for name in ('Arduino.h', 'SD_MMC.h', 'esp_heap_caps.h', 'esp_system.h',
                 'freertos/FreeRTOS.h', 'freertos/semphr.h'):
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "cache_fakes.h"\n')
    binary = work / 'cache'
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', f'-I{work}', f'-I{root}/tests', f'-I{root}/src',
                    str(root / 'tests/test_gps_cache.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
