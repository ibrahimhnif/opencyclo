import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo/core/ble/gps_assistance.dart';

Uint8List frame(int id, int type, int length) {
  final f = Uint8List(length + 8);
  f.setAll(0, [0xb5, 0x62, 0x13, id, length, 0, type]);
  ubxChecksum(f);
  return f;
}

void main() {
  test('valid live response; rejects corrupt, truncated and arbitrary UBX', () {
    final data = [...frame(0x40, 0x10, 24), ...frame(0, 1, 68)];
    expect(parseLiveAssistance(data).length, 2);
    expect(() => parseLiveAssistance(data.sublist(0, data.length - 1)),
        throwsFormatException);
    data[data.length - 1] ^= 1;
    expect(() => parseLiveAssistance(data), throwsFormatException);
    expect(
        () =>
            parseLiveAssistance([...frame(0x40, 0x10, 24), ...frame(4, 0, 4)]),
        throwsFormatException);
    expect(() => parseLiveAssistance(frame(0x40, 0x10, 24)),
        throwsFormatException);
  });
  test('server time preserved with conservative accuracy and valid checksum',
      () {
    final f = frame(0x40, 0x10, 24);
    final data = LiveAssistance([f, frame(0, 1, 68)], Stopwatch()..start());
    final adjusted = data.timeFrame();
    expect(adjusted.sublist(10, 22), f.sublist(10, 22));
    expect(ByteData.sublistView(adjusted).getUint16(22, Endian.little),
        greaterThanOrEqualTo(10));
    expect(parseLiveAssistance([...adjusted, ...data.frames[1]]).length, 2);
  });
  test(
      'MTU23 fragmentation retries are identical, progress waits for receiver ACK',
      () async {
    var index = 0, state = 3, retried = false;
    final collected = <int>[];
    final data = LiveAssistance(
        [frame(0x40, 0x10, 24), frame(0, 1, 68)], Stopwatch()..start());
    final progress = <double>[];
    await sendLiveAssistance(data, (p) async {
      expect(p.length, lessThanOrEqualTo(20));
      if (p[0] == 2) {
        final offset = p[7] + (p[8] << 8);
        if (offset == collected.length) collected.addAll(p.sublist(9));
        if (!retried) {
          retried = true;
          throw StateError('lost write reply');
        }
      }
      if (p[0] == 3) {
        expect(collected.length, data.frames[index].length);
        collected.clear();
        index++;
      }
      if (p[0] == 4) state = 6;
    }, () async => [1, state, 0, 0, 1, 0, 0, 0, index, 0, 0, 0], 23,
        progress.add,
        sessionToken: 1);
    expect(index, 2);
    expect(progress.last, 1);
    expect(retried, isTrue);
  });
  test('receiver rejection is not success and triggers abort', () async {
    final ops = <int>[];
    await expectLater(
        sendLiveAssistance(
            LiveAssistance(
                [frame(0x40, 0x10, 24), frame(0, 1, 68)], Stopwatch()..start()),
            (p) async {
          ops.add(p[0]);
        }, () async => [1, 7, 4, 5, 1, 0, 0, 0, 0, 0, 0, 0], 23,
            (_) => fail('must not report progress'),
            sessionToken: 1),
        throwsStateError);
    expect(ops.last, 5);
  });
}
