import 'dart:async';
import 'dart:typed_data';
import '../models/route_model.dart';

/// Async firmware returns BUSY while its SD worker runs. Older firmware's
/// immediate acknowledgements remain compatible. Never retry the write itself.
Future<String> waitForRouteReply(Future<String> Function() read,
    {Duration timeout = const Duration(seconds: 30),
    Duration interval = const Duration(milliseconds: 20)}) async {
  final elapsed = Stopwatch()..start();
  while (true) {
    final remaining = timeout - elapsed.elapsed;
    if (remaining <= Duration.zero) {
      throw TimeoutException('Device route operation timed out', timeout);
    }
    final reply = await read().timeout(remaining);
    if (reply != 'BUSY') {
      if (reply.startsWith('ERR')) throw StateError(reply);
      return reply;
    }
    await Future<void>.delayed(interval);
  }
}

/// Transport-independent protocol, so MTU boundaries, cancellation and device
/// rejection are testable without pretending a successful GATT write saved a file.
Future<void> transferRoute({
  required Uint8List bytes,
  required int mtu,
  required Future<String> Function(List<int>) command,
  required Future<String> Function(List<int>) data,
  required void Function(double) progress,
  bool Function()? cancelled,
}) async {
  try {
    if (mtu < 23) throw StateError('Invalid BLE MTU');
    final begin = ByteData(9)
      ..setUint8(0, 1)
      ..setUint32(1, bytes.length, Endian.little)
      ..setUint32(5, RouteModel.checksum(bytes), Endian.little);
    if (await command(begin.buffer.asUint8List()) != 'OK 0') {
      throw StateError('Device did not start transfer');
    }
    final chunk = (mtu - 7).clamp(1, 480);
    for (var offset = 0; offset < bytes.length;) {
      if (cancelled?.call() ?? false) throw StateError('Transfer cancelled');
      final end = (offset + chunk).clamp(0, bytes.length);
      final packet = ByteData(4 + end - offset)
        ..setUint32(0, offset, Endian.little);
      packet.buffer
          .asUint8List()
          .setRange(4, packet.lengthInBytes, bytes.sublist(offset, end));
      final ack = await data(packet.buffer.asUint8List());
      if (ack != 'OK $end') throw StateError('Route transfer rejected: $ack');
      offset = end;
      progress(offset / bytes.length * 0.99);
    }
    if (cancelled?.call() ?? false) throw StateError('Transfer cancelled');
    final result = await command([2]);
    if (!result.startsWith('SAVED ')) {
      throw StateError('Route was not saved: $result');
    }
    progress(1);
  } catch (_) {
    try {
      await command([3]);
    } catch (_) {/* A disconnect may prevent abort. */}
    rethrow;
  }
}
