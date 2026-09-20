"""Exercise the production decoder with the installed real TinyGPS++ parser."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
library = root / '.pio/libdeps/esp32-s3-devkitc-1/TinyGPSPlus/src'
with tempfile.TemporaryDirectory(prefix='opencyclo-gps-tests-') as temp:
    work = Path(temp)
    for name in ['Arduino.h', 'freertos/FreeRTOS.h', 'freertos/semphr.h']:
        target = work / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "gps_fakes.h"\n')
    binary = work / 'gps'
    # -Wno-implicit-fallthrough: GCC flags a fall-through inside the vendored
    # TinyGPS++ parser (clang does not), which -Werror would reject.
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-Wno-implicit-fallthrough',
                    '-fsanitize=undefined', '-DARDUINO=10800',
                    f'-I{work}', f'-I{root}/tests', f'-I{root}/src', f'-I{library}',
                    str(library / 'TinyGPS++.cpp'), str(root / 'tests/test_gps_decoder.cpp'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    quality = work / 'gps_quality'
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-Wno-implicit-fallthrough',
                    '-fsanitize=undefined', '-DARDUINO=10800',
                    f'-I{work}', f'-I{root}/tests', f'-I{root}/src', f'-I{library}',
                    str(library / 'TinyGPS++.cpp'), str(root / 'tests/test_gps_quality.cpp'),
                    '-o', str(quality)], check=True)
    subprocess.run([str(quality)], check=True)
    assistance = work / 'gps_assistance'
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', f'-I{root}/src',
                    str(root / 'tests/test_gps_assistance.cpp'),
                    '-o', str(assistance)], check=True)
    subprocess.run([str(assistance)], check=True)
