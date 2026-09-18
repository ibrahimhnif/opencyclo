import 'dart:async';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';

typedef AidWrite = Future<void> Function(List<int>);
typedef AidRead = Future<List<int>> Function();

void ubxChecksum(Uint8List bytes) {
  var a = 0, b = 0;
  for (var i = 2; i < bytes.length - 2; i++) {
    a = (a + bytes[i]) & 255;
    b = (b + a) & 255;
  }
  bytes[bytes.length - 2] = a;
  bytes[bytes.length - 1] = b;
}

List<Uint8List> parseLiveAssistance(List<int> data) {
  if (data.length > 65536) {
    throw const FormatException('GPS response too large');
  }
  final frames = <Uint8List>[];
  for (var offset = 0; offset < data.length;) {
    if (offset + 8 > data.length) {
      throw const FormatException('Truncated GPS data');
    }
    final n = data[offset + 4] + (data[offset + 5] << 8) + 8;
    if (n < 12 || n > 520 || offset + n > data.length) {
      throw const FormatException('Invalid GPS frame length');
    }
    final frame = Uint8List.fromList(data.sublist(offset, offset + n));
    final check = Uint8List.fromList(frame);
    ubxChecksum(check);
    final id = frame[3], type = frame[6];
    final allowed = frames.isEmpty
        ? id == 0x40 && type == 0x10 && n == 32 && frame[8] == 0
        : (id == 0 &&
                ((type == 1 && n == 76) ||
                    (type == 4 && n == 48) ||
                    (type == 5 && n == 28) ||
                    (type == 6 && n == 24))) ||
            (id == 2 && ((type == 1 && n == 84) || (type == 5 && n == 28)));
    if (frame[0] != 0xb5 ||
        frame[1] != 0x62 ||
        frame[2] != 0x13 ||
        frame[7] != 0 ||
        !allowed ||
        check[n - 2] != frame[n - 2] ||
        check[n - 1] != frame[n - 1]) {
      throw const FormatException('Unsupported or corrupt AssistNow data');
    }
    frames.add(frame);
    if (frames.length > 256) throw const FormatException('Too many GPS frames');
    offset += n;
  }
  if (frames.length < 2 || !frames.skip(1).any((f) => f[6] == 1)) {
    throw const FormatException('AssistNow response has no orbit data');
  }
  return frames;
}

class LiveAssistance {
  final List<Uint8List> frames;
  final Stopwatch age;
  LiveAssistance(this.frames, this.age);

  static Future<LiveAssistance> fetch(String chipcode) async {
    if (chipcode.trim().isEmpty) {
      throw StateError('Enter your receiver Chipcode');
    }
    final client = HttpClient()
      ..connectionTimeout = const Duration(seconds: 15);
    final age = Stopwatch()..start();
    try {
      final uri =
          Uri.https('assistnow.services.u-blox.com', '/GetAssistNowData.ashx', {
        'chipcode': chipcode.trim(),
        'gnss': 'gps,gal',
        'data': 'ulorb_l1,usvht,ukion',
      });
      final request =
          await client.getUrl(uri).timeout(const Duration(seconds: 15));
      request.followRedirects =
          false; // Never forward a credential to another host.
      final response =
          await request.close().timeout(const Duration(seconds: 15));
      if (response.statusCode != 200) {
        throw StateError(
            'AssistNow HTTP ${response.statusCode}. Check Chipcode and Live Orbits access.');
      }
      final data = BytesBuilder(copy: false);
      await (() async {
        await for (final chunk in response) {
          data.add(chunk);
          if (data.length > 65536) {
            throw const FormatException('GPS response too large');
          }
        }
      })()
          .timeout(const Duration(seconds: 15));
      return LiveAssistance(parseLiveAssistance(data.takeBytes()), age);
    } on SocketException {
      throw StateError('Cannot reach AssistNow. Check internet connection.');
    } on HandshakeException {
      throw StateError('AssistNow TLS connection failed.');
    } on HttpException {
      throw StateError('AssistNow network request failed.');
    } finally {
      client.close(force: true);
    }
  }

  // Keep server UTC, conservatively enlarge its uncertainty for the entire
  // request + BLE setup delay. Never substitute an unchecked phone clock.
  Uint8List timeFrame() {
    if (age.elapsedMilliseconds > 30000) {
      throw StateError('Assistance expired before injection. Sync again.');
    }
    final frame = Uint8List.fromList(frames.first);
    final view = ByteData.sublistView(frame);
    final accuracy = view.getUint16(22, Endian.little) +
        (age.elapsedMilliseconds / 1000).ceil() +
        10;
    view.setUint16(22, min(65535, accuracy), Endian.little);
    ubxChecksum(frame);
    return frame;
  }
}

Future<void> sendLiveAssistance(LiveAssistance data, AidWrite write,
    AidRead read, int mtu, void Function(double) progress,
    {bool Function()? cancelled, int? sessionToken}) async {
  final token = sessionToken ?? Random.secure().nextInt(0x7ffffffe) + 1;
  List<int> command(int op, [int? index, int? offset]) => [
        op,
        for (var i = 0; i < 4; i++) (token >> (8 * i)) & 255,
        if (index != null) ...[index & 255, index >> 8],
        if (offset != null) ...[offset & 255, offset >> 8],
      ];
  void checkCancel() {
    if (cancelled?.call() ?? false) throw StateError('GPS sync cancelled');
  }

  Future<void> send(List<int> packet) async {
    for (var attempt = 0;; attempt++) {
      checkCancel();
      try {
        await write(packet).timeout(const Duration(seconds: 3));
        return;
      } catch (_) {
        if (attempt == 2) {
          rethrow;
        }
      }
    }
  }

  Future<void> waitFor(int state, int index) async {
    final timer = Stopwatch()..start();
    while (timer.elapsedMilliseconds < 5000) {
      checkCancel();
      final bytes = await read().timeout(const Duration(seconds: 3));
      if (bytes.length != 12 || bytes[0] != 1) {
        throw StateError('Update OpenCyclo firmware for GPS sync');
      }
      final b = ByteData.sublistView(Uint8List.fromList(bytes));
      if (b.getUint32(4, Endian.little) != token) {
        throw StateError('GPS sync busy or connection changed');
      }
      if (bytes[1] == 7) {
        throw StateError('GPS assistance failed: ${const {
              1: 'receiver version not supported',
              2: 'invalid packet',
              3: 'receiver timeout',
              4: 'receiver rejected data',
              5: 'UART write failed',
              6: 'cancelled',
            }[bytes[2]] ?? 'unknown'} (receiver code ${bytes[3]})');
      }
      if (bytes[1] == state && b.getUint16(8, Endian.little) == index) return;
      await Future<void>.delayed(const Duration(milliseconds: 40));
    }
    throw TimeoutException('GPS acknowledgement timed out');
  }

  try {
    await send(command(1));
    await waitFor(3, 0);
    final chunk = max(1, min(180, mtu - 12));
    for (var index = 0; index < data.frames.length; index++) {
      final frameTimer = Stopwatch()..start();
      final frame = index == 0 ? data.timeFrame() : data.frames[index];
      for (var offset = 0; offset < frame.length; offset += chunk) {
        await send([
          ...command(2, index, offset),
          ...frame.sublist(offset, min(offset + chunk, frame.length))
        ]);
      }
      // Time packets delayed by a stalled BLE link must not be injected.
      if (index == 0 && frameTimer.elapsedMilliseconds > 5000) {
        throw StateError('Time assistance delayed. Sync again.');
      }
      await send(command(3, index));
      await waitFor(3, index + 1);
      progress((index + 1) / (data.frames.length + 1));
    }
    await send(command(4, data.frames.length));
    await waitFor(6, data.frames.length);
    progress(1);
  } finally {
    // DONE is retained; ABORT only cancels an active transfer. Bounded cleanup.
    try {
      await write(command(5)).timeout(const Duration(seconds: 2));
    } catch (_) {}
  }
}
