import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo/core/ble/gps_assistance.dart';
import 'package:opencyclo/core/ble/gps_registration.dart';

List<int> identityBytes() {
  final ver = Uint8List(78)..setAll(0, [0xb5, 0x62, 0x0a, 4, 70, 0]);
  ver.setAll(46, 'PROTVER=34.10'.codeUnits);
  ubxChecksum(ver);
  final uid = Uint8List(18)..setAll(0, [0xb5, 0x62, 0x27, 3, 10, 0, 2]);
  ubxChecksum(uid);
  return [78, 0, ...ver, ...uid];
}

void main() {
  test('full UBX identity frames retained including sync and checksum', () {
    final bytes = identityBytes();
    final id = GpsIdentity.decode(bytes);
    expect(id.messages['UBX-MON-VER']!.length, 156);
    expect(id.messages['UBX-SEC-UNIQID']!.startsWith('B5622703'), isTrue);
    bytes[bytes.length - 1] ^= 1;
    expect(() => GpsIdentity.decode(bytes), throwsFormatException);
  });
  test('token validation and response errors do not echo secrets', () {
    expect(normalizeZtpToken(' AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE '),
        'aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee');
    expect(() => normalizeZtpToken('secret'), throwsFormatException);
    expect(
        parseZtpResponse(jsonEncode(
            {'chipcode': 'test-code', 'allowedData': 'ulorb_l1,usvht,ukion'})),
        'test-code');
    expect(
        () => parseZtpResponse('{secret'),
        throwsA(
            isA<FormatException>().having((e) => e.source, 'source', isNull)));
    expect(
        () => parseZtpResponse(
            jsonEncode({'chipcode': 'secret', 'allowedData': 'ualm'})),
        throwsStateError);
  });
  test('identity download paginates MTU23 without exposing ZTP token to BLE',
      () async {
    final source = identityBytes();
    var offset = 0;
    final id = await readGpsIdentity((p) async {
      expect(p.length, lessThanOrEqualTo(7));
      if (p[0] == 2) offset = p[5] + (p[6] << 8);
    },
        () async => [
              1,
              5,
              0,
              7,
              0,
              0,
              0,
              offset & 255,
              offset >> 8,
              source.length,
              0,
              ...source.skip(offset).take(9)
            ],
        sessionToken: 7);
    expect(id.version.length, 78);
    expect(id.uniqueId.length, 18);
  });
  test('stale session and cancellation fail closed', () async {
    await expectLater(
        readGpsIdentity(
            (_) async {}, () async => [1, 5, 0, 8, 0, 0, 0, 0, 0, 0, 0],
            sessionToken: 7),
        throwsStateError);
    await expectLater(
        readGpsIdentity((_) async {}, () async => [],
            sessionToken: 7, cancelled: () => true),
        throwsStateError);
  });
}
