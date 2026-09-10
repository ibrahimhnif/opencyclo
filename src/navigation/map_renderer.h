#pragma once
enum class MapStatus { Loading, Ready, Missing, NoMemory };
// UI-only: request/coalesce a viewport and blit the latest completed raster.
MapStatus drawMapBackground(double x, double y, int zoom);
