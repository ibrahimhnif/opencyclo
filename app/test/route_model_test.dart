import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/models/route_model.dart';

void main() {
  test('namespaced GPX produces firmware-compatible packet and checksum', () {
    final r = RouteModel.fromGpx(
        '<g:gpx xmlns:g="urn:gpx"><g:trk><g:trkseg><g:trkpt lat="-6" lon="106"/><g:trkpt lat="-6.001" lon="106.001"/></g:trkseg></g:trk></g:gpx>',
        name: 'ride.gpx');
    final bytes = r.toBytes(), b = ByteData.sublistView(r.toBytes());
    expect(utf8.decode(bytes.take(4).toList()), 'OCR1');
    expect(b.getUint32(4, Endian.little), 2);
    expect(b.getInt32(60, Endian.little), -60000000);
    expect(bytes.length, 76);
    expect(r.distance, greaterThan(150));
    expect(RouteModel.checksum(utf8.encode('123456789')), 0xcbf43926);
  });
  test('rejects gaps, invalid coordinates and empty tracks', () {
    for (final text in [
      '<gpx/>',
      '<gpx><trk><trkseg/><trkseg/></trk></gpx>',
      '<gpx><rte><rtept lat="NaN" lon="1"/></rte></gpx>'
    ]) {
      expect(() => RouteModel.fromGpx(text), throwsFormatException);
    }
  });
  test('keeps provided cue, removes duplicate points, bounds text in packet',
      () {
    final r = RouteModel.fromGpx(
        '<gpx><rte><rtept lat="0" lon="0"/><rtept lat="0" lon="0"/><rtept lat="0" lon="0.001"><desc>Turn right</desc></rtept></rte></gpx>');
    expect(r.points.length, 2);
    expect(r.cues.single.text, 'Turn right');
    expect(r.cues.single.direction, 1);
    final bytes = r.toBytes();
    expect(bytes.length, 124);
    expect(bytes.last, 0);
  });
  test('geometry-only turns are explicitly labelled bends', () {
    final r = RouteModel.fromGpx(
        '<gpx><rte><rtept lat="0" lon="0"/><rtept lat="0.001" lon="0"/><rtept lat="0.001" lon="0.001"/></rte></gpx>');
    expect(r.cues.single.direction, 1);
    expect(r.cues.single.text, 'route bends right');
  });
}
