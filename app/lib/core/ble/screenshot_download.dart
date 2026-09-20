import 'dart:typed_data';
import 'dart:ui' as ui;
import 'ride_download.dart';

/// Control opcode 0x16: the device's most recent screenshot this boot, or
/// null when it has none. Names follow the same rules as ride files, with a
/// .bmp extension; the size is what the device will stream on open (0x17).
Future<SavedRide?> lastScreenshot(RideCommand command) async {
  String reply;
  try {
    reply = await command([0x16]);
  } on StateError catch (e) {
    if (e.toString().contains('ERR no screenshot')) return null;
    rethrow;
  }
  final match =
      RegExp(r'^SHOT (\d+) ([A-Za-z0-9_.-]+\.bmp)$').firstMatch(reply);
  if (match == null) throw FormatException('Invalid screenshot info: $reply');
  final name = match[2]!;
  if (name.contains('..') || name.length > 58) {
    throw const FormatException('Invalid screenshot name');
  }
  return SavedRide(name, int.parse(match[1]!));
}

/// Re-encodes the device's uncompressed 24-bit BMP as PNG through the
/// platform image codec, so the saved file is small and opens anywhere.
Future<Uint8List> bmpToPng(Uint8List bmp) async {
  final codec = await ui.instantiateImageCodec(bmp);
  try {
    final frame = await codec.getNextFrame();
    try {
      final png = await frame.image.toByteData(format: ui.ImageByteFormat.png);
      if (png == null) throw StateError('PNG encoding failed');
      return png.buffer.asUint8List();
    } finally {
      frame.image.dispose();
    }
  } finally {
    codec.dispose();
  }
}
