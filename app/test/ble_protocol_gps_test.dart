import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/ble/ble_protocol.dart';

void main() {
  test('encodePhoneGpsSample packs lat/lon/accuracy/seq into 11 bytes', () {
    final bytes = encodePhoneGpsSample(37.7749295, -122.4194155, 12.5, 42);
    expect(bytes.length, 11);

    final view = ByteData.sublistView(bytes);
    expect(view.getInt32(0, Endian.little) / 1e7, closeTo(37.7749295, 1e-6));
    expect(view.getInt32(4, Endian.little) / 1e7, closeTo(-122.4194155, 1e-6));
    expect(view.getUint16(8, Endian.little), 1250); // 12.5m -> centimeters
    expect(bytes[10], 42);
  });

  test('encodePhoneGpsSample clamps accuracy and wraps sequence to a byte', () {
    final bytes = encodePhoneGpsSample(0, 0, 1000.0, 300);
    final view = ByteData.sublistView(bytes);
    expect(view.getUint16(8, Endian.little), 65535); // clamped, not overflowed
    expect(bytes[10], 300 & 0xFF);
  });
}
