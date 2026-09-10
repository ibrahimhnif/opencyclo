import 'dart:convert';
import 'dart:math' as math;
import 'dart:typed_data';
import 'package:xml/xml.dart';

class RoutePoint {
  final double lat, lon;
  const RoutePoint(this.lat, this.lon);
  double distanceTo(RoutePoint p) {
    const r = math.pi / 180;
    final h = math.pow(math.sin((p.lat - lat) * r / 2), 2) +
        math.cos(lat * r) *
            math.cos(p.lat * r) *
            math.pow(math.sin((p.lon - lon) * r / 2), 2);
    return 12742000 * math.asin(math.sqrt(h.clamp(0, 1)));
  }
}

class RouteCue {
  final int point, direction;
  final String text;
  const RouteCue(this.point, this.direction, this.text);
}

class RouteModel {
  final String name;
  final List<RoutePoint> points;
  final List<RouteCue> cues;
  final List<double> distances;
  RouteModel(this.name, this.points, this.cues) : distances = [0] {
    for (var i = 1; i < points.length; i++) {
      distances.add(distances.last + points[i - 1].distanceTo(points[i]));
    }
  }
  double get distance => distances.last;
  static RouteModel fromGpx(String source, {String name = 'route'}) {
    if (utf8.encode(source).length > 8 * 1024 * 1024) {
      throw const FormatException('GPX exceeds 8 MB');
    }
    final doc = XmlDocument.parse(source);
    if (doc.rootElement.name.local != 'gpx') {
      throw const FormatException('Not a GPX document');
    }
    final elements = doc.descendants.whereType<XmlElement>();
    final segments = elements.where((e) => e.name.local == 'trkseg').toList();
    final routes = elements.where((e) => e.name.local == 'rte').toList();
    // Never silently join unrelated tracks or gaps with a navigable straight line.
    if (segments.length > 1 || (segments.isEmpty && routes.length > 1)) {
      throw const FormatException(
          'Choose a GPX with one continuous track segment or route');
    }
    final container = segments.isNotEmpty
        ? segments.single
        : routes.isNotEmpty
            ? routes.single
            : null;
    if (container == null) {
      throw const FormatException('GPX has no track or route');
    }
    final kind = segments.isNotEmpty ? 'trkpt' : 'rtept';
    final ps = <RoutePoint>[];
    final explicit = <RouteCue>[];
    for (final e
        in container.childElements.where((e) => e.name.local == kind)) {
      final lat = double.tryParse(e.getAttribute('lat') ?? '');
      final lon = double.tryParse(e.getAttribute('lon') ?? '');
      if (lat == null ||
          lon == null ||
          !lat.isFinite ||
          !lon.isFinite ||
          lat.abs() > 85 ||
          lon.abs() > 180) {
        throw const FormatException('Invalid or unsupported coordinates');
      }
      if (ps.isNotEmpty && (ps.last.lon - lon).abs() > 180) {
        throw const FormatException('Antimeridian routes are unsupported');
      }
      final p = RoutePoint(lat, lon);
      if (ps.isEmpty || ps.last.distanceTo(p) >= 0.5) {
        ps.add(p);
      }
      if (ps.length > 12000) {
        throw const FormatException(
            'Route exceeds 12,000 points; simplify it before importing');
      }
      final descriptions = e.childElements.where((e) => e.name.local == 'desc');
      if (descriptions.isNotEmpty &&
          descriptions.first.innerText.trim().isNotEmpty) {
        final text = descriptions.first.innerText.trim();
        final direction = RegExp(r'\b(left|kiri)\b', caseSensitive: false)
                .hasMatch(text)
            ? -1
            : RegExp(r'\b(right|kanan)\b', caseSensitive: false).hasMatch(text)
                ? 1
                : 0;
        explicit.add(RouteCue(ps.length - 1, direction, text));
      }
    }
    if (ps.length < 2) {
      throw const FormatException('Route needs at least two distinct points');
    }
    final result = RouteModel(
        name.replaceFirst(RegExp(r'\.gpx$', caseSensitive: false), ''),
        ps,
        explicit);
    if (explicit.isEmpty) {
      // Geometric hints are labelled as bends, not verified junction decisions.
      double last = -100;
      for (var i = 1; i < ps.length - 1; i++) {
        var a = i - 1, b = i + 1;
        while (a > 0 && result.distances[i] - result.distances[a] < 25) {
          a--;
        }
        while (b < ps.length - 1 &&
            result.distances[b] - result.distances[i] < 25) {
          b++;
        }
        final c = math.cos(ps[i].lat * math.pi / 180);
        final before =
            math.atan2((ps[i].lon - ps[a].lon) * c, ps[i].lat - ps[a].lat);
        final after =
            math.atan2((ps[b].lon - ps[i].lon) * c, ps[b].lat - ps[i].lat);
        final angle = (after - before + 3 * math.pi) % (2 * math.pi) - math.pi;
        if (angle.abs() > 0.65 && result.distances[i] - last > 60) {
          result.cues.add(RouteCue(i, angle > 0 ? 1 : -1,
              angle > 0 ? 'route bends right' : 'route bends left'));
          last = result.distances[i];
        }
      }
    }
    if (result.cues.length > 256) {
      throw const FormatException('Route exceeds 256 instructions');
    }
    return result;
  }

  Uint8List toBytes() {
    final b = ByteData(60 + points.length * 8 + cues.length * 48);
    void ascii(int offset, String text, int capacity) {
      final safe = text.replaceAll(RegExp(r'[^\x20-\x7e]'), '?').codeUnits;
      for (var i = 0; i < safe.length && i < capacity - 1; i++) {
        b.setUint8(offset + i, safe[i]);
      }
    }

    ascii(0, 'OCR1', 5);
    b.setUint32(4, points.length, Endian.little);
    b.setUint32(8, cues.length, Endian.little);
    ascii(12, name, 48);
    for (var i = 0; i < points.length; i++) {
      b.setInt32(60 + i * 8, (points[i].lat * 1e7).round(), Endian.little);
      b.setInt32(64 + i * 8, (points[i].lon * 1e7).round(), Endian.little);
    }
    for (var i = 0; i < cues.length; i++) {
      final o = 60 + points.length * 8 + i * 48;
      b.setUint32(o, cues[i].point, Endian.little);
      b.setInt8(o + 4, cues[i].direction);
      ascii(o + 5, cues[i].text, 43);
    }
    return b.buffer.asUint8List();
  }

  static int checksum(List<int> bytes) {
    var crc = 0xffffffff;
    for (final b in bytes) {
      crc ^= b;
      for (var i = 0; i < 8; i++) {
        crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xedb88320 : 0);
      }
    }
    return (crc ^ 0xffffffff) & 0xffffffff;
  }
}
