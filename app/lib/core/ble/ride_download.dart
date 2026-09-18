import 'dart:convert';
import 'dart:typed_data';
import '../models/route_model.dart';

class SavedRide {
  final String name;
  final int size;
  const SavedRide(this.name, this.size);
}

typedef RideCommand = Future<String> Function(List<int>);

List<int> rideRequest(int command, int value, [String name = '']) {
  final header = ByteData(5)
    ..setUint8(0, command)
    ..setUint32(1, value, Endian.little);
  return [...header.buffer.asUint8List(), ...ascii.encode(name)];
}

Future<List<SavedRide>> listSavedRides(RideCommand command) async {
  final rides = <SavedRide>[];
  for (var i = 0; i <= 4096; i++) {
    final reply = await command(rideRequest(0x10, i));
    if (reply == 'END') return rides;
    final match =
        RegExp(r'^FILE (\d+) ([A-Za-z0-9_.-]+\.gpx)$').firstMatch(reply);
    if (match == null) throw FormatException('Invalid ride list: $reply');
    final name = match[2]!;
    if (name.contains('..') || name.length > 58) {
      throw const FormatException('Invalid ride name');
    }
    rides.add(SavedRide(name, int.parse(match[1]!)));
  }
  throw StateError('Ride library exceeds 4096 entries');
}

Future<Uint8List> downloadRide(
    SavedRide ride, RideCommand command, void Function(double) progress,
    {bool Function()? cancelled}) async {
  if (ride.size <= 0 || ride.size > 32 * 1024 * 1024) {
    throw StateError('Ride is empty or exceeds 32 MB');
  }
  final bytes = BytesBuilder(copy: false);
  while (bytes.length < ride.size) {
    if (cancelled?.call() ?? false) throw StateError('Download cancelled');
    final reply = await command(rideRequest(0x11, bytes.length, ride.name));
    final match =
        RegExp(r'^DATA (\d+) ([0-9a-f]{8}) ([0-9a-f]+)$').firstMatch(reply);
    if (match == null ||
        int.parse(match[1]!) != bytes.length ||
        match[3]!.length.isOdd) {
      throw const FormatException('Invalid ride chunk or offset');
    }
    final hex = match[3]!;
    final chunk = Uint8List.fromList([
      for (var i = 0; i < hex.length; i += 2)
        int.parse(hex.substring(i, i + 2), radix: 16)
    ]);
    if (chunk.isEmpty ||
        chunk.length > 160 ||
        bytes.length + chunk.length > ride.size ||
        RouteModel.checksum(chunk) != int.parse(match[2]!, radix: 16)) {
      throw const FormatException('Ride checksum or size mismatch');
    }
    bytes.add(chunk);
    progress(bytes.length / ride.size);
  }
  // Preserve original bytes, including older or interrupted GPX logs, for debugging.
  return bytes.takeBytes();
}
