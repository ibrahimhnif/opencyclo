import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/ble/ride_download.dart';
import 'package:opencyclo_app/core/models/route_model.dart';

void main() {
  test('saved ride list paginates and rejects unsafe names', () async {
    final rides = await listSavedRides(
        (request) async => request[1] == 0 ? 'FILE 42 ride.gpx' : 'END');
    expect(rides.single.size, 42);
    await expectLater(listSavedRides((_) async => 'FILE 42 ../ride.gpx'),
        throwsFormatException);
  });
  test('download preserves all bytes with offsets and CRCs', () async {
    final original = Uint8List.fromList(List.generate(401, (i) => i % 256));
    var previous = 0.0;
    final actual =
        await downloadRide(const SavedRide('ride.gpx', 401), (request) async {
      final offset = ByteData.sublistView(Uint8List.fromList(request))
          .getUint32(1, Endian.little);
      final chunk =
          original.sublist(offset, (offset + 160).clamp(0, original.length));
      final crc = RouteModel.checksum(chunk).toRadixString(16).padLeft(8, '0');
      final hex = chunk.map((b) => b.toRadixString(16).padLeft(2, '0')).join();
      return 'DATA $offset $crc $hex';
    }, (progress) {
      expect(progress, greaterThan(previous));
      previous = progress;
    });
    expect(actual, original);
    expect(previous, 1);
  });
  test('bad offset, checksum, EOF and cancellation fail closed', () async {
    for (final reply in [
      'DATA 1 00000000 00',
      'DATA 0 00000000 00',
      'END',
      'DATA 0 00000000 0'
    ]) {
      await expectLater(
          downloadRide(
              const SavedRide('ride.gpx', 1), (_) async => reply, (_) {}),
          throwsFormatException);
    }
    await expectLater(
        downloadRide(const SavedRide('ride.gpx', 1),
            (_) async => throw StateError('must not call'), (_) {},
            cancelled: () => true),
        throwsStateError);
  });
}
