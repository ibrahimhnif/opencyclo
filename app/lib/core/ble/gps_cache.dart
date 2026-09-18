import 'dart:async';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';
import 'gps_assistance.dart';

int cacheCrc(List<int> bytes) {
  var c = 0xffffffff;
  for (final b in bytes) {
    c ^= b;
    for (var i = 0; i < 8; i++) {
      c = (c >> 1) ^ ((c & 1) != 0 ? 0xedb88320 : 0);
    }
  }
  return (c ^ 0xffffffff) & 0xffffffff;
}

class PredictiveCache {
  final Uint8List bytes;
  final DateTime first, last;
  final DateTime serverUtc;
  final Stopwatch age;
  PredictiveCache._(
      this.bytes, this.first, this.last, this.serverUtc, this.age);
  factory PredictiveCache.parse(
      List<int> bytes, DateTime serverUtc, Stopwatch age) {
    if (bytes.isEmpty || bytes.length > 131072 || bytes.length % 84 != 0) {
      throw const FormatException('Invalid predictive cache size');
    }
    DateTime? first, last;
    for (var i = 0; i < bytes.length; i += 84) {
      final f = Uint8List.fromList(bytes.sublist(i, i + 84));
      final copy = Uint8List.fromList(f);
      ubxChecksum(copy);
      if (f[0] != 0xb5 ||
          f[1] != 0x62 ||
          f[2] != 0x13 ||
          f[3] != 0x20 ||
          f[4] != 76 ||
          f[5] != 0 ||
          f[6] != 0 ||
          f[7] != 0 ||
          (f[9] != 0 && f[9] != 2) ||
          f[82] != copy[82] ||
          f[83] != copy[83]) {
        throw const FormatException('Corrupt predictive orbit frame');
      }
      final d = DateTime.utc(2000 + f[10], f[11], f[12]);
      if (d.year != 2000 + f[10] ||
          d.month != f[11] ||
          d.day != f[12] ||
          d.year < 2020 ||
          d.year > 2099) {
        throw const FormatException('Invalid orbit date');
      }
      if (first == null || d.isBefore(first)) first = d;
      if (last == null || d.isAfter(last)) last = d;
    }
    final today = DateTime.utc(serverUtc.year, serverUtc.month, serverUtc.day);
    if (last!.difference(first!).inDays > 6 ||
        today.isBefore(first) ||
        today.isAfter(last)) {
      throw const FormatException(
          'Predictive orbits expired or outside requested week');
    }
    return PredictiveCache._(
        Uint8List.fromList(bytes), first, last, serverUtc, age);
  }
  static Future<PredictiveCache> fetch(String chipcode) async {
    if (chipcode.trim().isEmpty) {
      throw StateError('Load or register a Chipcode first');
    }
    final client = HttpClient()
      ..connectionTimeout = const Duration(seconds: 15);
    final timer = Stopwatch()..start();
    try {
      final request = await client
          .getUrl(Uri.https(
              'assistnow.services.u-blox.com', '/GetAssistNowData.ashx', {
            'chipcode': chipcode.trim(),
            'gnss': 'gps,gal',
            'data': 'uporb_7'
          }))
          .timeout(const Duration(seconds: 15));
      request.followRedirects = false;
      request.headers.set(HttpHeaders.cacheControlHeader, 'no-cache');
      final response =
          await request.close().timeout(const Duration(seconds: 15));
      if (response.statusCode != 200) {
        throw StateError(
            'Predictive HTTP ${response.statusCode}. Enable Predictive Orbits in your profile.');
      }
      DateTime server;
      try {
        server =
            HttpDate.parse(response.headers.value(HttpHeaders.dateHeader) ?? '')
                .toUtc();
      } catch (_) {
        throw StateError('Server time unavailable; cache not saved');
      }
      final data = BytesBuilder(copy: false);
      await (() async {
        await for (final chunk in response) {
          data.add(chunk);
          if (data.length > 131072) {
            throw const FormatException('Cache too large');
          }
        }
      })()
          .timeout(const Duration(seconds: 15));
      if (timer.elapsedMilliseconds > 30000) {
        throw StateError('Server response too slow; retry sync');
      }
      // Age starts at response completion; HTTP request latency is covered by device uncertainty.
      return PredictiveCache.parse(
          data.takeBytes(), server, Stopwatch()..start());
    } on SocketException {
      throw StateError('Check internet connection');
    } on HandshakeException {
      throw StateError('Predictive TLS connection failed');
    } on HttpException {
      throw StateError('Predictive download failed');
    } finally {
      client.close(force: true);
    }
  }
}

class GpsCacheStatus {
  final int transfer, status, error, token, received, first, last;
  GpsCacheStatus(List<int> bytes)
      : transfer = bytes[1],
        status = bytes[2],
        error = bytes[3],
        token = ByteData.sublistView(Uint8List.fromList(bytes))
            .getUint32(4, Endian.little),
        received = ByteData.sublistView(Uint8List.fromList(bytes))
            .getUint32(8, Endian.little),
        first = ByteData.sublistView(Uint8List.fromList(bytes))
            .getUint32(12, Endian.little),
        last = ByteData.sublistView(Uint8List.fromList(bytes))
            .getUint32(16, Endian.little);
  static GpsCacheStatus parse(List<int> p) {
    if (p.length != 20 || p[0] != 1) {
      throw StateError('Update firmware for GPS cache');
    }
    return GpsCacheStatus(p);
  }

  String get label =>
      const {
        0: 'perlu sync',
        1: 'menunggu waktu',
        2: 'cache siap',
        3: 'memakai cache',
        4: 'cache dipakai',
        5: 'perlu sync — kedaluwarsa',
        6: 'SD unavailable',
        7: 'cache error — sync ulang'
      }[status] ??
      'unknown';
  String get expiry => last == 0
      ? ''
      : DateTime.fromMillisecondsSinceEpoch(last * 86400000, isUtc: true)
          .toIso8601String()
          .substring(0, 10);
}

Future<void> uploadGpsCache(PredictiveCache cache, AidWrite write, AidRead read,
    int mtu, void Function(double) progress,
    {bool Function()? cancelled, int? sessionToken}) async {
  final token = sessionToken ?? Random.secure().nextInt(0x7ffffffe) + 1;
  List<int> u32(int n) => [for (var i = 0; i < 4; i++) (n >> (8 * i)) & 255];
  List<int> cmd(int op) => [op, ...u32(token)];
  Future<void> send(List<int> p) async {
    if (cancelled?.call() ?? false) throw StateError('Cache sync cancelled');
    await write(p).timeout(const Duration(seconds: 3));
  }

  Future<GpsCacheStatus> status() async {
    final s =
        GpsCacheStatus.parse(await read().timeout(const Duration(seconds: 3)));
    if (s.token != token) {
      throw StateError('Finish ride or wait for GPS to be idle');
    }
    if (s.transfer == 4) {
      throw StateError('Cache save failed (code ${s.error}). Check SD card.');
    }
    return s;
  }

  try {
    await send(
        [...cmd(1), ...u32(cache.bytes.length), ...u32(cacheCrc(cache.bytes))]);
    await status();
    final chunk = max(1, min(180, mtu - 12));
    for (var offset = 0; offset < cache.bytes.length; offset += chunk) {
      final end = min(offset + chunk, cache.bytes.length);
      await send(
          [...cmd(2), ...u32(offset), ...cache.bytes.sublist(offset, end)]);
      if ((await status()).received != end) {
        throw StateError('Cache transfer interrupted');
      }
      progress(end / (cache.bytes.length + 1));
    }
    await send(cmd(3));
    final timer = Stopwatch()..start();
    while ((await status()).transfer != 3) {
      if (cancelled?.call() ?? false) throw StateError('Cache sync cancelled');
      if (timer.elapsedMilliseconds > 15000) {
        throw StateError('Cache save timed out; finish ride and check SD');
      }
      await Future<void>.delayed(const Duration(milliseconds: 100));
    }
    // Fresh server time plus monotonic elapsed, never a persisted old time value.
    if (cache.age.elapsed.inMinutes < 10) {
      final utc =
          cache.serverUtc.add(cache.age.elapsed).millisecondsSinceEpoch ~/ 1000;
      await send([...cmd(4), ...u32(utc)]);
    }
    progress(1);
  } finally {
    try {
      await write(cmd(5)).timeout(const Duration(seconds: 2));
    } catch (_) {}
  }
}
