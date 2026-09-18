import 'dart:async';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/ble/ride_download_fast.dart';
import 'package:opencyclo_app/core/ble/ride_download.dart';
import 'package:opencyclo_app/core/models/route_model.dart';

void main() {
  for (final failure in [
    'ERR export MTU 12',
    'ERR export MTU 180',
    'ERR export backpressure'
  ]) {
    test('retry server negotiation/backpressure: $failure', () async {
      final stream = StreamController<List<int>>.broadcast(sync: true);
      int windows = 0;
      bool closed = false;
      final messages = <String>[];
      final result =
          await downloadRideFast(const SavedRide('ride.gpx', 3), (r) async {
        if (r[0] == 0x15) throw StateError('ERR command');
        if (r[0] == 0x12) return 'FAST 9 3';
        if (r[0] == 0x14) {
          closed = true;
          return 'OK closed';
        }
        if (++windows == 1) throw StateError(failure);
        final request = ByteData.sublistView(Uint8List.fromList(r));
        expect(request.getUint16(9, Endian.little),
            failure.contains('MTU') ? int.parse(failure.split(' ').last) : 480);
        stream.add([9, 0, 0, 0, 0, 0, 0, 0, 97, 98, 99]);
        return 'BLOCK 0 3 352441c2';
      }, stream.stream, 512, (_) {}, diagnostic: messages.add);
      expect(result, [97, 98, 99]);
      expect(windows, 2);
      expect(closed, true);
      expect(messages, isNotEmpty);
      await stream.close();
    });
  }
  for (final credits in [16, 64]) {
    for (final mtu in [23, 185, 512]) {
      for (final corrupt in [false, true]) {
        test(
            'binary export credits=$credits mtu=$mtu corruptFirst=$corrupt preserves bytes',
            () async {
          final stream = StreamController<List<int>>.broadcast(sync: true);
          final source =
              Uint8List.fromList(List.generate(16001, (i) => i % 256));
          var windows = 0, closed = false;
          final progress = <double>[];
          Future<String> command(List<int> request) async {
            if (request[0] == 0x15) return 'CAPS $credits';
            final data = ByteData.sublistView(Uint8List.fromList(request));
            if (request[0] == 0x12) return 'FAST 7 ${source.length}';
            if (request[0] == 0x14) {
              closed = true;
              return 'OK closed';
            }
            expect(data.getUint32(1, Endian.little), 7);
            final start = data.getUint32(5, Endian.little);
            final payload = data.getUint16(9, Endian.little);
            expect(request.length, credits == 16 ? 11 : 12);
            if (credits != 16) expect(request[11], credits);
            final end = (start + payload * credits).clamp(0, source.length);
            windows++;
            // Deliberately reverse notifications: assembly is offset-based.
            final packets = <Uint8List>[];
            for (int at = start; at < end; at += payload) {
              final n = (end - at).clamp(0, payload);
              final packet = Uint8List(n + 8);
              ByteData.sublistView(packet)
                ..setUint32(0, 7, Endian.little)
                ..setUint32(4, at, Endian.little);
              packet.setRange(8, packet.length, source, at);
              if (corrupt && windows == 1) packet[8] ^= 1;
              packets.add(packet);
            }
            for (final packet in packets.reversed) {
              stream.add(packet);
              stream.add(packet);
            }
            return 'BLOCK $start $end ${RouteModel.checksum(Uint8List.sublistView(source, start, end)).toRadixString(16).padLeft(8, '0')}';
          }

          final got = await downloadRideFast(
              SavedRide('ride.gpx', source.length),
              command,
              stream.stream,
              mtu,
              progress.add);
          expect(got, source);
          expect(closed, true);
          expect(progress.last, 1);
          final payload = (mtu - 11).clamp(1, 480);
          expect(windows,
              (source.length / (payload * credits)).ceil() + (corrupt ? 1 : 0));
          await stream.close();
        });
      }
    }
  }
  test('missing notifications retry boundedly then close without success',
      () async {
    final stream = StreamController<List<int>>.broadcast();
    var requests = 0, closed = false;
    Future<String> command(List<int> r) async {
      if (r[0] == 0x15) return 'CAPS 16';
      if (r[0] == 0x12) return 'FAST 1 3';
      if (r[0] == 0x14) {
        closed = true;
        return 'OK closed';
      }
      requests++;
      return 'BLOCK 0 3 352441c2';
    }

    await expectLater(
        downloadRideFast(const SavedRide('ride.gpx', 3), command, stream.stream,
            23, (_) => fail('must not report completed progress')),
        throwsStateError);
    expect(requests, 3);
    expect(closed, true);
    await stream.close();
  });
  test('cancel releases the open session', () async {
    final stream = StreamController<List<int>>.broadcast();
    var closed = false;
    await expectLater(
        downloadRideFast(const SavedRide('ride.gpx', 3), (r) async {
          if (r[0] == 0x15) return 'CAPS 16';
          if (r[0] == 0x12) return 'FAST 1 3';
          expect(r[0], 0x14);
          closed = true;
          return 'OK closed';
        }, stream.stream, 23, (_) {}, cancelled: () => true),
        throwsStateError);
    expect(closed, true);
    await stream.close();
  });
}
