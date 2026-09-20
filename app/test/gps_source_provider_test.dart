import 'dart:async';
import 'package:flutter_test/flutter_test.dart';
import 'package:geolocator/geolocator.dart';
import 'package:opencyclo/core/location/phone_gps_service.dart';
import 'package:opencyclo/state/gps_source_provider.dart';

class FakeGpsSourceBleChannel implements GpsSourceBleChannel {
  int? modeToReturnOnRead;
  final List<int> writtenModes = [];
  final List<List<num>> writtenSamples = [];
  final _controller = StreamController<int>.broadcast();
  final _connectionController = StreamController<bool>.broadcast();

  @override
  Stream<int> get gpsSourceModeStream => _controller.stream;

  @override
  Stream<bool> get isConnectedStream => _connectionController.stream;

  void setConnected(bool connected) => _connectionController.add(connected);

  @override
  Future<int?> readGpsSourceMode() async => modeToReturnOnRead;

  @override
  Future<void> writeGpsSourceMode(int mode) async => writtenModes.add(mode);

  @override
  Future<void> writePhoneGpsSample(
      double lat, double lon, double accuracyM, int seq, int utcEpochS) async {
    writtenSamples.add([lat, lon, accuracyM, seq, utcEpochS]);
  }

  void dispose() {
    _controller.close();
    _connectionController.close();
  }
}

/// The real permission check goes through Geolocator's platform channel, which
/// does not exist under `flutter test`. The foreground service calls need no
/// equivalent: the notifier skips them off Android/iOS.
class PermittedPhoneGpsService extends PhoneGpsService {
  PermittedPhoneGpsService(PositionStreamFactory factory)
      : super(positionStreamFactory: factory);

  @override
  Future<bool> ensurePermission() async => true;
}

Position _fakePosition(double lat, double lon, double accuracy,
        {DateTime? timestamp}) =>
    Position(
      latitude: lat,
      longitude: lon,
      timestamp: timestamp ?? DateTime.now(),
      accuracy: accuracy,
      altitude: 0,
      altitudeAccuracy: 0,
      heading: 0,
      headingAccuracy: 0,
      speed: 0,
      speedAccuracy: 0,
    );

/// Lets every pending microtask (the connect -> permission -> subscribe chain)
/// run before the assertion.
Future<void> _settle() async {
  await Future<void>.delayed(Duration.zero);
  await Future<void>.delayed(Duration.zero);
}

void main() {
  test('loadFromDevice reflects the mode read from the device', () async {
    final channel = FakeGpsSourceBleChannel()..modeToReturnOnRead = 1;
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: const PhoneGpsService(
          positionStreamFactory: _emptyPositionStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    await notifier.loadFromDevice();
    expect(notifier.state.mode, GpsSourceMode.phoneForced);
  });

  test('setMode writes the mode byte to the device', () async {
    final channel = FakeGpsSourceBleChannel();
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: const PhoneGpsService(
          positionStreamFactory: _emptyPositionStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    await notifier.setMode(GpsSourceMode.phoneForced);
    expect(channel.writtenModes, [1]);
    expect(notifier.state.mode, GpsSourceMode.phoneForced);
  });

  test('a device-side mode change (Notify) updates state', () async {
    final channel = FakeGpsSourceBleChannel();
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: const PhoneGpsService(
          positionStreamFactory: _emptyPositionStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    channel._controller.add(1);
    await Future<void>.delayed(Duration.zero);
    expect(notifier.state.mode, GpsSourceMode.phoneForced);
  });

  test('connecting starts streaming position to the device, in hardware mode',
      () async {
    final channel = FakeGpsSourceBleChannel();
    final positions = StreamController<Position>.broadcast();
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: PermittedPhoneGpsService(() => positions.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      positions.close();
    });

    // Default mode is hardware -- firmware's auto-fallback needs a phone
    // sample already queued, so the app must stream in this mode too.
    expect(notifier.state.mode, GpsSourceMode.hardware);

    channel.setConnected(true);
    await _settle();
    final fixedTimestamp = DateTime.utc(2026, 9, 18, 11, 59, 30);
    positions.add(
        _fakePosition(37.7749, -122.4194, 8.0, timestamp: fixedTimestamp));
    await _settle();

    expect(channel.writtenSamples, isNotEmpty);
    expect(channel.writtenSamples.first[0], 37.7749);
    expect(channel.writtenSamples.first[1], -122.4194);
    expect(channel.writtenSamples.first[2], 8.0);
    expect(channel.writtenSamples.first[3], 1); // seq starts at 1
    expect(channel.writtenSamples.first[4],
        fixedTimestamp.millisecondsSinceEpoch ~/ 1000);
    expect(notifier.state.error, isNull);
  });

  test('disconnecting stops streaming position', () async {
    final channel = FakeGpsSourceBleChannel();
    final positions = StreamController<Position>.broadcast();
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: PermittedPhoneGpsService(() => positions.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      positions.close();
    });

    channel.setConnected(true);
    await _settle();
    positions.add(_fakePosition(1, 2, 3));
    await _settle();
    expect(channel.writtenSamples.length, 1);

    channel.setConnected(false);
    await _settle();
    positions.add(_fakePosition(4, 5, 6));
    await _settle();

    expect(channel.writtenSamples.length, 1,
        reason: 'no writes after the BLE link dropped');
  });

  test('a position stream error is surfaced on state instead of thrown',
      () async {
    final channel = FakeGpsSourceBleChannel();
    final positions = StreamController<Position>.broadcast();
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: PermittedPhoneGpsService(() => positions.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      positions.close();
    });

    channel.setConnected(true);
    await _settle();
    positions.addError(StateError('location unavailable'));
    await _settle();

    expect(notifier.state.error, contains('location unavailable'));
  });

  test('a refused location permission is surfaced and blocks streaming',
      () async {
    final channel = FakeGpsSourceBleChannel();
    final positions = StreamController<Position>.broadcast();
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: _DeniedPhoneGpsService(() => positions.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      positions.close();
    });

    channel.setConnected(true);
    await _settle();
    positions.add(_fakePosition(1, 2, 3));
    await _settle();

    expect(notifier.state.error, contains('Location permission'));
    expect(channel.writtenSamples, isEmpty);
  });
}

class _DeniedPhoneGpsService extends PhoneGpsService {
  _DeniedPhoneGpsService(PositionStreamFactory factory)
      : super(positionStreamFactory: factory);

  @override
  Future<bool> ensurePermission() async => false;
}

Stream<Position> _emptyPositionStream() => const Stream.empty();
