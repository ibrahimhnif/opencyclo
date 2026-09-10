#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace nav {
constexpr double pi = 3.14159265358979323846;
constexpr double world = 4194304.0; // Web Mercator pixels at zoom 14
struct Point { int32_t lat, lon; }; // degrees * 1e7, little endian on disk
inline bool valid(Point p) { return p.lat >= -850000000 && p.lat <= 850000000 && p.lon >= -1800000000 && p.lon <= 1800000000; }
inline double x(Point p) { return (p.lon / 1e7 + 180.0) / 360.0 * world; }
inline double y(Point p) { double a = p.lat / 1e7 * pi / 180; return (1 - std::log(std::tan(a) + 1 / std::cos(a)) / pi) / 2 * world; }
inline double distance(Point a, Point b) {
  double dlat = (double(b.lat) - a.lat) / 1e7 * pi / 180;
  double dlon = (double(b.lon) - a.lon) / 1e7 * pi / 180;
  double h = std::pow(std::sin(dlat/2),2) + std::cos(a.lat/1e7*pi/180)*std::cos(b.lat/1e7*pi/180)*std::pow(std::sin(dlon/2),2);
  return 12742000 * std::asin(std::sqrt(h > 1 ? 1 : h));
}
inline double segmentDistance(Point p, Point a, Point b, double& fraction) {
  double c = std::cos(p.lat / 1e7 * pi / 180);
  double ax=(double(a.lon)-p.lon)*c, ay=double(a.lat)-p.lat;
  double bx=(double(b.lon)-a.lon)*c, by=double(b.lat)-a.lat;
  double n=bx*bx+by*by;
  fraction = n ? -(ax*bx+ay*by)/n : 0;
  if (fraction<0) fraction=0;
  if (fraction>1) fraction=1;
  return std::hypot(ax+fraction*bx, ay+fraction*by)*0.011119493;
}
inline uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc=0xffffffff) {
  for(size_t i=0;i<size;i++) { crc ^= data[i]; for(int j=0;j<8;j++) crc=(crc>>1)^((crc&1)?0xedb88320:0); }
  return crc;
}
constexpr uint32_t maxPoints = 12000;
constexpr uint32_t maxCues = 256;
#pragma pack(push, 1)
struct Header { char magic[4]; uint32_t points, cues; char name[48]; };
struct Cue { uint32_t point; int8_t direction; char text[43]; };
#pragma pack(pop)
inline bool headerValid(const Header& h, size_t size) {
  return !std::memcmp(h.magic,"OCR1",4) && h.points>=2 && h.points<=maxPoints && h.cues<=maxCues &&
    size==sizeof(Header)+h.points*sizeof(Point)+h.cues*sizeof(Cue) && h.name[47]==0;
}
}
