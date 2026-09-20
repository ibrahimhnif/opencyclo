import 'dart:async';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/ble/ride_download.dart';
import 'package:opencyclo_app/core/ble/ride_download_fast.dart';
import 'package:opencyclo_app/core/ble/screenshot_download.dart';

/// 2x1 24-bit bottom-up BMP: left pixel red, right pixel blue. Rows are
/// B,G,R and padded to 4 bytes (6 -> 8), so the file is 54 + 8 bytes.
Uint8List bmp2x1() {
  final b = ByteData(62);
  b.setUint8(0, 0x42); b.setUint8(1, 0x4D);
  b.setUint32(2, 62, Endian.little);
  b.setUint32(10, 54, Endian.little);
  b.setUint32(14, 40, Endian.little);
  b.setInt32(18, 2, Endian.little);
  b.setInt32(22, 1, Endian.little);
  b.setUint16(26, 1, Endian.little);
  b.setUint16(28, 24, Endian.little);
  b.setUint32(34, 8, Endian.little);
  const pixels = [0, 0, 255, 255, 0, 0, 0, 0]; // red (B,G,R), blue, padding
  for (var i = 0; i < pixels.length; i++) {
    b.setUint8(54 + i, pixels[i]);
  }
  return b.buffer.asUint8List();
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('lastScreenshot parses SHOT <size> <name> from command 0x16', () async {
    final shot = await lastScreenshot((r) async {
      expect(r, [0x16]);
      return 'SHOT 230454 20260920_083012.bmp';
    });
    expect(shot, isNotNull);
    expect(shot!.name, '20260920_083012.bmp');
    expect(shot.size, 230454);
  });

  test('lastScreenshot is null when the device has none', () async {
    final shot = await lastScreenshot(
        (r) async => throw StateError('ERR no screenshot'));
    expect(shot, isNull);
  });

  test('lastScreenshot rejects traversal and non-bmp names', () async {
    expect(() => lastScreenshot((r) async => 'SHOT 3 ../x.bmp'),
        throwsFormatException);
    expect(() => lastScreenshot((r) async => 'SHOT 3 ride.gpx'),
        throwsFormatException);
  });

  test('downloadRideFast opens with 0x17 when asked, never 0x12', () async {
    final stream = StreamController<List<int>>.broadcast(sync: true);
    int opened = 0;
    final result = await downloadRideFast(
        const SavedRide('20260920_083012.bmp', 3), (r) async {
      if (r[0] == 0x15) return 'CAPS 64';
      if (r[0] == 0x12) fail('ride opener used for a screenshot');
      if (r[0] == 0x17) {
        opened = r[0];
        expect(r.sublist(5), '20260920_083012.bmp'.codeUnits);
        return 'FAST 9 3';
      }
      if (r[0] == 0x14) return 'OK closed';
      stream.add([9, 0, 0, 0, 0, 0, 0, 0, 97, 98, 99]);
      return 'BLOCK 0 3 352441c2';
    }, stream.stream, 512, (_) {}, openCommand: 0x17);
    expect(result, [97, 98, 99]);
    expect(opened, 0x17);
    await stream.close();
  });

  test('bmpToPng keeps the pixels of a 24-bit bottom-up BMP', () async {
    final png = await bmpToPng(bmp2x1());
    expect(png.sublist(0, 8), [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]);
    final codec = await ui.instantiateImageCodec(png);
    final frame = await codec.getNextFrame();
    expect(frame.image.width, 2);
    expect(frame.image.height, 1);
    final rgba = (await frame.image.toByteData(format: ui.ImageByteFormat.rawRgba))!
        .buffer
        .asUint8List();
    expect(rgba.sublist(0, 4), [255, 0, 0, 255]);
    expect(rgba.sublist(4, 8), [0, 0, 255, 255]);
  });
}
