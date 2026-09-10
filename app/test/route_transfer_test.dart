import 'dart:async';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/ble/route_transfer.dart';
import 'package:opencyclo_app/core/models/route_model.dart';

void main() {
  test('worker BUSY is polled until ACK, including legacy immediate ACK',
      () async {
    final replies = ['BUSY', 'BUSY', 'OK 16'];
    expect(
        await waitForRouteReply(() async => replies.removeAt(0),
            interval: Duration.zero),
        'OK 16');
    expect(replies, isEmpty);
    expect(await waitForRouteReply(() async => 'SAVED /routes/test.ocr'),
        'SAVED /routes/test.ocr');
  });
  test('worker errors, stalled work and disconnect do not become success',
      () async {
    await expectLater(
        waitForRouteReply(() async => 'ERR SD busy'), throwsStateError);
    await expectLater(
        waitForRouteReply(() async => 'BUSY',
            timeout: const Duration(milliseconds: 5), interval: Duration.zero),
        throwsA(isA<TimeoutException>()));
    await expectLater(
        waitForRouteReply(() => Completer<String>().future,
            timeout: const Duration(milliseconds: 5)),
        throwsA(isA<TimeoutException>()));
    await expectLater(
        waitForRouteReply(() async => throw StateError('disconnected')),
        throwsStateError);
  });
  test(
      'minimum MTU transfers offset frames and waits for saved acknowledgement',
      () async {
    final bytes = Uint8List.fromList(List.generate(101, (i) => i));
    final received = <int>[], progress = <double>[];
    var saved = false;
    await transferRoute(
        bytes: bytes,
        mtu: 23,
        command: (v) async {
          if (v[0] == 1) {
            final b = ByteData.sublistView(Uint8List.fromList(v));
            expect(b.getUint32(5, Endian.little), RouteModel.checksum(bytes));
            return 'OK 0';
          }
          if (v[0] == 2) {
            expect(received, bytes);
            expect(progress.last, lessThan(1));
            saved = true;
            return 'SAVED /routes/test.ocr';
          }
          return 'OK cancelled';
        },
        data: (v) async {
          expect(v.length, lessThanOrEqualTo(20));
          expect(
              ByteData.sublistView(Uint8List.fromList(v))
                  .getUint32(0, Endian.little),
              received.length);
          received.addAll(v.skip(4));
          return 'OK ${received.length}';
        },
        progress: progress.add);
    expect(saved, isTrue);
    expect(progress.last, 1);
  });
  test('bad offset, failed commit and cancellation abort without success',
      () async {
    for (final failure in ['offset', 'commit', 'cancel']) {
      var aborted = false;
      final progress = <double>[];
      await expectLater(
          transferRoute(
              bytes: Uint8List(10),
              mtu: 512,
              command: (v) async {
                if (v[0] == 3) {
                  aborted = true;
                  return 'OK cancelled';
                }
                if (v[0] == 1) return 'OK 0';
                return 'ERR checksum';
              },
              data: (v) async => failure == 'offset' ? 'OK 0' : 'OK 10',
              progress: progress.add,
              cancelled: () => failure == 'cancel'),
          throwsStateError);
      expect(aborted, isTrue);
      expect(progress, isNot(contains(1.0)));
    }
  });
}
