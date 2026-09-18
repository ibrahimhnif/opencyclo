import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';
import 'gps_assistance.dart';

class GpsIdentity {
  final Uint8List version, uniqueId;
  GpsIdentity._(this.version, this.uniqueId);
  static void _validate(Uint8List f, int cls, int id) {
    if (f.length < 8 ||
        f[0] != 0xb5 ||
        f[1] != 0x62 ||
        f[2] != cls ||
        f[3] != id ||
        f[4] + (f[5] << 8) + 8 != f.length) {
      throw const FormatException('Invalid receiver identity frame');
    }
    final copy = Uint8List.fromList(f);
    ubxChecksum(copy);
    if (copy[copy.length - 2] != f[f.length - 2] || copy.last != f.last) {
      throw const FormatException('Receiver identity checksum failed');
    }
  }

  factory GpsIdentity.decode(List<int> data) {
    if (data.length < 2 || data.length > 540) {
      throw const FormatException('Invalid identity size');
    }
    final n = data[0] + (data[1] << 8);
    if (n < 48 || n > 520 || n + 2 >= data.length) {
      throw const FormatException('Invalid version size');
    }
    final version = Uint8List.fromList(data.sublist(2, n + 2));
    final unique = Uint8List.fromList(data.sublist(n + 2));
    _validate(version, 0x0a, 4);
    _validate(unique, 0x27, 3);
    if ((n - 48) % 30 != 0 ||
        !((unique.length == 17 && unique[6] == 1) ||
            (unique.length == 18 && unique[6] == 2))) {
      throw const FormatException('Unsupported receiver identity format');
    }
    final extensions = <String>[];
    for (var i = 46; i < version.length - 2; i += 30) {
      extensions.add(String.fromCharCodes(
          version.sublist(i, i + 30).takeWhile((b) => b != 0)));
    }
    if (!extensions.contains('PROTVER=34.10')) {
      throw StateError(
          'GPS firmware not supported by this AssistNow implementation');
    }
    return GpsIdentity._(version, unique);
  }
  Map<String, String> get messages => {
        'UBX-MON-VER': version
            .map((b) => b.toRadixString(16).padLeft(2, '0'))
            .join()
            .toUpperCase(),
        'UBX-SEC-UNIQID': uniqueId
            .map((b) => b.toRadixString(16).padLeft(2, '0'))
            .join()
            .toUpperCase(),
      };
}

Future<GpsIdentity> readGpsIdentity(AidWrite write, AidRead read,
    {bool Function()? cancelled, int? sessionToken}) async {
  final token = sessionToken ?? Random.secure().nextInt(0x7ffffffe) + 1;
  List<int> cmd(int op, [int? offset]) => [
        op,
        for (var i = 0; i < 4; i++) (token >> (8 * i)) & 255,
        if (offset != null) ...[offset & 255, offset >> 8]
      ];
  final timer = Stopwatch()..start();
  final result = <int>[];
  int? total;
  try {
    await write(cmd(1)).timeout(const Duration(seconds: 3));
    while (timer.elapsedMilliseconds < 20000) {
      if (cancelled?.call() ?? false) {
        throw StateError('Registration cancelled');
      }
      await write(cmd(2, result.length)).timeout(const Duration(seconds: 3));
      final p = await read().timeout(const Duration(seconds: 3));
      if (p.length < 11 || p.length > 20 || p[0] != 1) {
        throw StateError('Update firmware for GPS registration');
      }
      final b = ByteData.sublistView(Uint8List.fromList(p));
      if (b.getUint32(3, Endian.little) != token) {
        throw StateError('GPS busy or device changed');
      }
      if (p[1] == 6) {
        throw StateError(
            'GPS identity failed (code ${p[2]}). Check receiver connection.');
      }
      if (p[1] == 5) {
        final size = b.getUint16(9, Endian.little);
        if (size == 0 ||
            size > 540 ||
            (total != null && total != size) ||
            b.getUint16(7, Endian.little) != result.length ||
            p.length == 11 ||
            result.length + p.length - 11 > size) {
          throw const FormatException('Corrupt identity transfer');
        }
        total = size;
        result.addAll(p.sublist(11));
        if (result.length == size) return GpsIdentity.decode(result);
      } else {
        await Future<void>.delayed(const Duration(milliseconds: 50));
      }
    }
    throw TimeoutException('GPS identity timed out');
  } finally {
    try {
      await write(cmd(3)).timeout(const Duration(seconds: 2));
    } catch (_) {}
  }
}

String normalizeZtpToken(String token) {
  final value = token.trim().toLowerCase();
  if (!RegExp(r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')
      .hasMatch(value)) {
    throw const FormatException(
        'Enter a valid ZTP token from your device profile');
  }
  return value;
}

String parseZtpResponse(String body) {
  // Never expose response body, parser source, credentials or server diagnostics.
  try {
    final json = jsonDecode(body);
    if (json is! Map<String, dynamic>) throw const FormatException();
    final code = json['chipcode'], allowed = json['allowedData'];
    if (code is! String ||
        code.isEmpty ||
        code.length > 512 ||
        !RegExp(r'^[A-Za-z0-9+/=_-]+$').hasMatch(code) ||
        allowed is! String) {
      throw const FormatException();
    }
    if (!allowed.split(',').map((s) => s.trim()).contains('ulorb_l1')) {
      throw StateError(
          'Device registered, but Live Orbits access is unavailable. Check your profile.');
    }
    return code;
  } on StateError {
    rethrow;
  } catch (_) {
    throw const FormatException('Invalid ZTP response');
  }
}

Future<String> registerGpsIdentity(String token, GpsIdentity identity) async {
  final value = normalizeZtpToken(token);
  final client = HttpClient()..connectionTimeout = const Duration(seconds: 15);
  try {
    final request = await client
        .postUrl(Uri.https('api.thingstream.io', '/ztp/assistnow/credentials'))
        .timeout(const Duration(seconds: 15));
    request.followRedirects = false;
    request.headers.contentType = ContentType.json;
    request.write(jsonEncode({'token': value, 'messages': identity.messages}));
    final response = await request.close().timeout(const Duration(seconds: 15));
    if (response.statusCode != 200) {
      throw StateError(
          'ZTP HTTP ${response.statusCode}. Check token, profile and device eligibility.');
    }
    final bytes = BytesBuilder(copy: false);
    await (() async {
      await for (final chunk in response) {
        bytes.add(chunk);
        if (bytes.length > 8192) {
          throw const FormatException('ZTP response too large');
        }
      }
    })()
        .timeout(const Duration(seconds: 15));
    try {
      return parseZtpResponse(utf8.decode(bytes.takeBytes()));
    } on FormatException {
      throw const FormatException('Invalid ZTP response');
    }
  } on SocketException {
    throw StateError('Cannot reach registration service. Check internet.');
  } on HandshakeException {
    throw StateError('ZTP TLS connection failed.');
  } on HttpException {
    throw StateError('ZTP network request failed.');
  } finally {
    client.close(force: true);
  }
}
