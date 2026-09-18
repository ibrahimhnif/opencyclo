import 'dart:async';
import 'dart:typed_data';
import '../models/route_model.dart';
import 'ride_download.dart';

/// Credit-based binary transfer: one command requests at most 16 notifications.
/// Offset + session ID reject stale packets; each complete window has a CRC.
Future<Uint8List> downloadRideFast(SavedRide ride, RideCommand command,
    Stream<List<int>> notifications, int mtu, void Function(double) progress,
    {bool Function()? cancelled, void Function(String)? diagnostic}) async {
  if (ride.size <= 0 || ride.size > 32 * 1024 * 1024) {
    throw StateError('Ride is empty or exceeds 32 MB');
  }
  var payload = (mtu - 11).clamp(1, 480);
  var credits = 16;
  final result = Uint8List(ride.size);
  int token = 0, windowStart = 0, windowEnd = 0;
  final packets = <int, Uint8List>{};
  int rejected = 0, received = 0, lastLength = 0;
  final subscription = notifications.listen((value) {
    received++;
    lastLength = value.length;
    if (value.length <= 8 || value.length > payload + 8 || token == 0) return;
    final data = Uint8List.fromList(value);
    final header = ByteData.sublistView(data);
    final offset = header.getUint32(4, Endian.little);
    if (header.getUint32(0, Endian.little) != token ||
        offset < windowStart ||
        offset >= windowEnd ||
        (offset - windowStart) % payload != 0 ||
        value.length - 8 != (windowEnd - offset).clamp(0, payload)) {
      rejected++;
      return;
    }
    packets[offset] = Uint8List.sublistView(data, 8);
  });
  try {
    try {
      final caps = await command([0x15]);
      final match = RegExp(r'^CAPS (\d+)$').firstMatch(caps);
      if (match != null) credits = int.parse(match[1]!).clamp(1, 64);
    } on StateError catch (e) {
      // Previous binary-export firmware has no capability command.
      if (!e.toString().contains('ERR command')) rethrow;
    }
    final open = await command(rideRequest(0x12, ride.size, ride.name));
    final match = RegExp(r'^FAST (\d+) (\d+)$').firstMatch(open);
    if (match == null) throw FormatException('Invalid export session: $open');
    token = int.parse(match[1]!);
    if (token == 0 || int.parse(match[2]!) != ride.size) {
      throw const FormatException('Ride changed before download');
    }
    while (windowStart < ride.size) {
      windowEnd = (windowStart + payload * credits).clamp(0, ride.size);
      bool valid = false;
      for (var attempt = 0; attempt < 3 && !valid; attempt++) {
        if (cancelled?.call() ?? false) throw StateError('Download cancelled');
        packets.clear();
        received = rejected = lastLength = 0;
        final request = ByteData(credits == 16 ? 11 : 12)
          ..setUint8(0, 0x13)
          ..setUint32(1, token, Endian.little)
          ..setUint32(5, windowStart, Endian.little)
          ..setUint16(9, payload, Endian.little);
        if (credits != 16) request.setUint8(11, credits);
        String reply;
        try {
          reply = await command(request.buffer.asUint8List());
        } on StateError catch (e) {
          final limit =
              RegExp(r'ERR export MTU (\d+)').firstMatch(e.toString());
          if (limit != null) {
            final next = int.parse(limit[1]!);
            if (next < 1 || next >= payload) rethrow;
            payload = next;
            windowEnd = (windowStart + payload * credits).clamp(0, ride.size);
            diagnostic?.call('Export MTU adjusted: payload=$payload');
            continue;
          }
          if (e.toString().contains('ERR export backpressure')) {
            diagnostic?.call('Export queue busy; retry ${attempt + 1}');
            await Future<void>.delayed(const Duration(milliseconds: 100));
            continue;
          }
          rethrow;
        }
        final block =
            RegExp(r'^BLOCK (\d+) (\d+) ([0-9a-f]{8})$').firstMatch(reply);
        if (block == null ||
            int.parse(block[1]!) != windowStart ||
            int.parse(block[2]!) != windowEnd) {
          throw const FormatException('Invalid export window');
        }
        final expected = ((windowEnd - windowStart) / payload).ceil();
        for (var wait = 0; packets.length < expected && wait < 50; wait++) {
          if (cancelled?.call() ?? false) {
            throw StateError('Download cancelled');
          }
          await Future<void>.delayed(const Duration(milliseconds: 10));
        }
        if (packets.length != expected) {
          diagnostic?.call(
              'Export offset=$windowStart attempt=${attempt + 1} mtu=$mtu payload=$payload packets=${packets.length}/$expected received=$received rejected=$rejected lastLength=$lastLength');
          continue;
        }
        for (final packet in packets.entries) {
          result.setRange(
              packet.key, packet.key + packet.value.length, packet.value);
        }
        valid = RouteModel.checksum(
                Uint8List.sublistView(result, windowStart, windowEnd)) ==
            int.parse(block[3]!, radix: 16);
      }
      if (!valid) {
        throw StateError(
            'Download interrupted at $windowStart: received ${packets.length} packets, payload $payload (MTU $mtu). Retry export.');
      }
      if (cancelled?.call() ?? false) throw StateError('Download cancelled');
      windowStart = windowEnd;
      progress(windowStart / ride.size);
    }
    return result;
  } finally {
    await subscription.cancel();
    if (token != 0) {
      try {
        await command(rideRequest(0x14, token));
      } catch (_) {
        // Disconnect/30-second inactivity also release firmware ownership.
      }
    }
  }
}
