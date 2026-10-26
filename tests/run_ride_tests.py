"""Compile production ride lifecycle and touch UI with fake peripherals."""
from pathlib import Path
import subprocess
import tempfile
import sys
import os
import difflib

GOLDEN_DIR = Path(__file__).resolve().parent / "golden"
UPDATE_GOLDEN = os.environ.get("UPDATE_GOLDEN") == "1"
golden_checked = []


def check_golden(name, binary, args):
    """Byte-compare a preview binary's draw stream against tests/golden/<name>.txt."""
    actual = subprocess.check_output([str(binary), *args], text=True)
    path = GOLDEN_DIR / f"{name}.txt"
    if UPDATE_GOLDEN:
        GOLDEN_DIR.mkdir(exist_ok=True)
        path.write_text(actual)
    expected = path.read_text() if path.exists() else ""
    if actual != expected:
        diff = list(difflib.unified_diff(expected.splitlines(), actual.splitlines(),
                                         f"golden/{name}.txt", "actual", lineterm=""))
        print("\n".join(diff[:40]))
        sys.exit(f"golden mismatch: {name}")
    golden_checked.append(name)

root = Path(__file__).resolve().parents[1]
# Theme resolution and view models every component-drawing build links.
UI_THEME_SOURCES = [str(root/"src/ui/engine/theme.cpp"), str(root/"src/ui/engine/resolved_theme.cpp"),
                    str(root/"src/ui/engine/widget_model.cpp")]
COMPONENT_SOURCES = [str(p) for p in sorted((root/"src/ui/components").glob("*.cpp"))]
with tempfile.TemporaryDirectory(prefix="opencyclo-ride-tests-") as temp:
    work = Path(temp)
    for header in ["Arduino.h", "freertos/FreeRTOS.h", "freertos/semphr.h", "hardware/display.h",
                   "ui/frame_present.h", "ui/gfx.h"]:
        target = work / header
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "ride_fakes.h"\n')
    # Fake navigation entry point (the real header pulls in NimBLE).
    nav=work/"navigation/navigation.h";nav.parent.mkdir(parents=True,exist_ok=True)
    nav.write_text('void openNavigation();\n')
    # Everything the ride menu links now that it draws with components and
    # sends ride commands through actions.cpp.
    RIDE_MENU_SOURCES=[str(root/"src/ui/ride_menu.cpp"),str(root/"src/ui/actions.cpp"),
                       str(root/"tests/ui_action_stubs.cpp"),*UI_THEME_SOURCES,*COMPONENT_SOURCES]
    binary = work / "ride"
    subprocess.run([
        "c++", "-std=c++11", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
        f"-I{work}", f"-I{root}/tests", f"-I{root}/src",
        str(root / "src/core/telemetry_state.cpp"),
        *RIDE_MENU_SOURCES,
        str(root / "tests/test_ride_session.cpp"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True, timeout=30)
    actions=work/"actions"
    subprocess.run([
        "c++","-std=c++11","-Wall","-Wextra","-Werror",
        f"-I{work}",f"-I{root}/tests",f"-I{root}/src",
        str(root/"src/core/telemetry_state.cpp"),str(root/"src/ui/actions.cpp"),
        str(root/"tests/test_actions.cpp"),"-o",str(actions),
    ],check=True)
    subprocess.run([str(actions)],check=True)
    resolved=work/"resolved_theme"
    subprocess.run([
        "c++","-std=c++11","-Wall","-Wextra","-Werror",
        f"-I{work}",f"-I{root}/tests",f"-I{root}/src",
        str(root/"src/ui/engine/theme.cpp"),str(root/"src/ui/engine/resolved_theme.cpp"),
        str(root/"tests/test_resolved_theme.cpp"),"-o",str(resolved),
    ],check=True)
    subprocess.run([str(resolved)],check=True)
    components=work/"components"
    subprocess.run([
        "c++","-std=c++11","-Wall","-Wextra","-Werror",
        f"-I{work}",f"-I{root}/tests",f"-I{root}/src",
        *UI_THEME_SOURCES, *COMPONENT_SOURCES,
        str(root/"tests/test_components.cpp"),"-o",str(components),
    ],check=True)
    subprocess.run([str(components)],check=True)
    preview=work/"preview"
    subprocess.run([
        "c++", "-std=c++11", f"-I{work}", f"-I{root}/tests", f"-I{root}/src",
        str(root/"src/core/telemetry_state.cpp"),*RIDE_MENU_SOURCES,
        str(root/"tests/test_ride_preview.cpp"),"-o",str(preview),
    ],check=True)
    check_golden("ride", preview, [])
    if len(sys.argv)>1:
        commands=subprocess.check_output([str(preview)],text=True)
        subprocess.run([sys.executable,str(root/"tests/render_ride_preview.py"),sys.argv[1]],
                       input=commands,text=True,check=True)
    for header in ["hardware/display.h", "hardware/battery.h", "hardware/ble_task.h",
                   "hardware/ble_camera_remote.h", "hardware/camera_control.h",
                   "hardware/ble_layout_sync.h",
                   "storage/settings.h", "ui/power_menu.h"]:
        target=work/header
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text('#include "widget_fakes.h"\n')
    camera=work/"camera_ui"
    subprocess.run([
        "c++","-std=c++11","-Wall","-Wextra","-Werror",
        f"-I{work}",f"-I{root}/tests",f"-I{root}/src",
        str(root/"src/ui/engine/widget_registry.cpp"),
        str(root/"src/ui/engine/widget_catalog.cpp"),
        str(root/"src/ui/engine/widget_model.cpp"),
        str(root/"src/ui/engine/theme.cpp"),
        str(root/"src/hardware/camera_protocol.cpp"),
        str(root/"src/ui/engine/resolved_theme.cpp"),*COMPONENT_SOURCES,
        str(root/"tests/test_camera_ui.cpp"),"-o",str(camera),
    ],check=True)
    subprocess.run([str(camera)],check=True)
    sensor_ui=work/"sensor_ui"
    subprocess.run([
        "c++","-std=c++11","-Wall","-Wextra","-Werror",
        f"-I{work}",f"-I{root}/tests",f"-I{root}/src",
        str(root/"src/ui/engine/widget_registry.cpp"),
        str(root/"src/ui/engine/widget_catalog.cpp"),
        str(root/"src/ui/engine/widget_model.cpp"),
        str(root/"src/ui/engine/theme.cpp"),
        str(root/"src/hardware/camera_protocol.cpp"),
        str(root/"src/ui/engine/resolved_theme.cpp"),*COMPONENT_SOURCES,
        str(root/"tests/test_sensor_ui.cpp"),"-o",str(sensor_ui),
    ],check=True)
    subprocess.run([str(sensor_ui)],check=True)
    check_golden("sensors", sensor_ui, ["preview"])
    check_golden("camera", camera, ["preview"])
    if len(sys.argv)>1:
        commands=subprocess.check_output([str(sensor_ui),"preview"],text=True)
        subprocess.run([sys.executable,str(root/"tests/render_ride_preview.py"),str(Path(sys.argv[1])/"sensors")],input=commands,text=True,check=True)
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
        str(root/"src/ui/engine/widget_model.cpp"),str(root/"src/ui/engine/theme.cpp"),
        str(root/"src/ui/engine/template_engine.cpp"),str(root/"src/ui/engine/layout_manager.cpp"),
        str(root/"src/hardware/camera_protocol.cpp"),
        str(root/"src/ui/engine/resolved_theme.cpp"),*COMPONENT_SOURCES,
        str(root/"tests/test_page_ui.cpp"),"-o",str(pages)],check=True)
    subprocess.run([str(pages)],check=True)
    check_golden("pages", pages, ["preview"])
    if len(sys.argv)>1:
        commands=subprocess.check_output([str(pages),"preview"],text=True)
        subprocess.run([sys.executable,str(root/"tests/render_ride_preview.py"),str(Path(sys.argv[1])/"pages")],input=commands,text=True,check=True)
    print("golden: " + ", ".join(golden_checked) + " match")
