#pragma once
#include <cstddef>
enum class MapStatus { Loading, Ready, Missing, NoMemory };
// UI-only: request/coalesce a viewport and blit the latest completed raster.
MapStatus drawMapBackground(double x, double y, int zoom);
// Looks up the nearest named road within 40m of (lat,lon) using the OCN1
// data for that position's map tile (current tile only, no cross-tile-
// boundary search). Returns true and NUL-terminates `name` (writing at
// most nameLen-1 characters + terminator) on a match; returns false and
// leaves `name` untouched otherwise (no .ocn file for this column, no
// match within range, or an invalid/absent GPS fix).
bool nearestRoadName(double lat, double lon, char* name, size_t nameLen);
