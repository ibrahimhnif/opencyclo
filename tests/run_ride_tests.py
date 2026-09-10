"""Compile production ride lifecycle and touch UI with fake peripherals."""
from pathlib import Path
import subprocess
import tempfile
import sys

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
    if len(sys.argv)>1:
        preview=work/"preview"
        subprocess.run([
            "c++", "-std=c++11", f"-I{work}", f"-I{root}/tests", f"-I{root}/src",
            str(root/"src/core/telemetry_state.cpp"),str(root/"src/ui/ride_menu.cpp"),
            str(root/"tests/test_ride_preview.cpp"),"-o",str(preview),
        ],check=True)
        commands=subprocess.check_output([str(preview)],text=True)
        subprocess.run([sys.executable,str(root/"tests/render_ride_preview.py"),sys.argv[1]],
                       input=commands,text=True,check=True)
    for header in ["hardware/display.h", "hardware/battery.h", "hardware/ble_task.h",
                   "hardware/ble_camera_remote.h", "storage/settings.h", "ui/power_menu.h"]:
        target=work/header
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text('#include "widget_fakes.h"\n')
    camera=work/"camera_ui"
    subprocess.run([
        "c++","-std=c++11","-Wall","-Wextra","-Werror",
        f"-I{work}",f"-I{root}/tests",f"-I{root}/src",
        str(root/"src/ui/engine/widget_registry.cpp"),
        str(root/"src/ui/engine/widget_catalog.cpp"),
        str(root/"tests/test_camera_ui.cpp"),"-o",str(camera),
    ],check=True)
    subprocess.run([str(camera)],check=True)
    if len(sys.argv)>1:
        commands=subprocess.check_output([str(camera),"preview"],text=True)
        subprocess.run([sys.executable,str(root/"tests/render_ride_preview.py"),str(Path(sys.argv[1])/"camera")],
                       input=commands,text=True,check=True)
    nav=work/"navigation/navigation.h";nav.parent.mkdir(parents=True,exist_ok=True)
    nav.write_text('void openNavigation();\n')
    pages=work/"page_ui"
    subprocess.run(["c++","-std=c++11","-Wall","-Wextra","-Werror",
        f"-I{work}",f"-I{root}/tests",f"-I{root}/src",
        str(root/"src/ui/engine/widget_registry.cpp"),str(root/"src/ui/engine/widget_catalog.cpp"),
        str(root/"src/ui/engine/template_engine.cpp"),str(root/"src/ui/engine/layout_manager.cpp"),
        str(root/"tests/test_page_ui.cpp"),"-o",str(pages)],check=True)
    subprocess.run([str(pages)],check=True)
    if len(sys.argv)>1:
        commands=subprocess.check_output([str(pages),"preview"],text=True)
        subprocess.run([sys.executable,str(root/"tests/render_ride_preview.py"),str(Path(sys.argv[1])/"pages")],input=commands,text=True,check=True)
