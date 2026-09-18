import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:opencyclo_app/core/ble/gps_cache.dart';
import 'package:opencyclo_app/core/ble/gps_credentials.dart';
import 'package:opencyclo_app/core/ble/gps_assistance.dart';
import 'package:opencyclo_app/core/ble/gps_registration.dart';
import 'gps_registration_test.dart' show identityBytes;

Uint8List orbit(int day) {
  final f = Uint8List(84)
    ..setAll(0, [0xb5, 0x62, 0x13, 0x20, 76, 0, 0, 0, 1, 0, 26, 9, day]);
  ubxChecksum(f);
  return f;
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  test('seven-day bounds, corrupt bytes, invalid dates and expiry', () {
    final now = DateTime.utc(2026, 9, 13, 12);
    final data = [for (var d = 13; d < 20; d++) ...orbit(d)];
    final cache = PredictiveCache.parse(data, now, Stopwatch()..start());
    expect(cache.last, DateTime.utc(2026, 9, 19));
    expect(
        () =>
            PredictiveCache.parse(data, DateTime.utc(2026, 9, 20), Stopwatch()),
        throwsFormatException);
    expect(
        () => PredictiveCache.parse([...data, ...orbit(20)], now, Stopwatch()),
        throwsFormatException);
    data[20] ^= 1;
    expect(() => PredictiveCache.parse(data, now, Stopwatch()),
        throwsFormatException);
    expect(cacheCrc('123456789'.codeUnits), 0xcbf43926);
  });
  test('secure credential persistence, silicon isolation and targeted forget',
      () async {
    FlutterSecureStorage.setMockInitialValues({});
    const store = GpsCredentials();
    final a = GpsIdentity.decode(identityBytes());
    final bBytes = identityBytes();
    bBytes[bBytes.length - 3] = 1;
    final uid = Uint8List.fromList(bBytes.sublist(80));
    ubxChecksum(uid);
    bBytes.setRange(80, bBytes.length, uid);
    final b = GpsIdentity.decode(bBytes);
    await store.save(a, 'credential-a');
    expect(await store.load(a), 'credential-a');
    expect(await store.load(b), isNull);
    await store.save(b, 'credential-b');
    await store.forget(a);
    expect(await store.load(a), isNull);
    expect(await store.load(b), 'credential-b');
  });
  test('upload reports success only after persisted ACK and sends fresh UTC',
      () async {
    final cache = PredictiveCache.parse(
        orbit(13), DateTime.utc(2026, 9, 13, 12), Stopwatch()..start());
    var received = 0, state = 1;
    var sentTime = false;
    final values = <double>[];
    List<int> u32(int n) => [for (var i = 0; i < 4; i++) (n >> (i * 8)) & 255];
    await uploadGpsCache(cache, (p) async {
      expect(p.length, lessThanOrEqualTo(20));
      if (p[0] == 2) received += p.length - 9;
      if (p[0] == 3) state = 3;
      if (p[0] == 4) sentTime = true;
    },
        () async => [
              1,
              state,
              1,
              0,
              ...u32(7),
              ...u32(received),
              ...u32(20709),
              ...u32(20715)
            ],
        23,
        values.add,
        sessionToken: 7);
    expect(values.last, 1);
    expect(sentTime, isTrue);
    expect(received, 84);
  });
  test('SD failure never reaches 100 percent', () async {
    final cache = PredictiveCache.parse(
        orbit(13), DateTime.utc(2026, 9, 13, 12), Stopwatch()..start());
    await expectLater(
        uploadGpsCache(
            cache,
            (_) async {},
            () async =>
                [1, 4, 6, 4, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            23,
            (_) => fail('false success'),
            sessionToken: 7),
        throwsStateError);
  });
}
