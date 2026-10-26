#pragma once
// The draw target every component takes. On the device this is LovyanGFX's
// common base, so components draw into the canvas sprite or the panel alike;
// host test runners substitute this header with their recording fake canvas.
#include "hardware/display.h"

using Gfx = lgfx::LovyanGFX;
using GfxFont = const lgfx::IFont*;
