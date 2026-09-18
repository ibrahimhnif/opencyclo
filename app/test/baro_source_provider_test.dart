import 'dart:async';
import 'package:flutter_test/flutter_test.dart';
import 'package:sensors_plus/sensors_plus.dart';
import 'package:opencyclo_app/core/location/phone_baro_service.dart';
import 'package:opencyclo_app/state/baro_source_provider.dart';

class FakeBaroSourceBleChannel implements BaroSourceBleChannel {
  int? modeToReturnOnRead;
  final List<int> writtenModes = [];
  final List<List<num>> writtenSamples = [];
  final _controller = StreamController<int>.broadcast();
  final _connectionController = StreamController<bool>.broadcast();

  @override
  Stream<int> get baroSourceModeStream => _controller.stream;

  @override
  Stream<bool> get isConnectedStream => _connectionController.stream;

  void setConnected(bool connected) => _connectionController.add(connected);

  @override
  Future<int?> readBaroSourceMode() async => modeToReturnOnRead;

  @override
  Future<void> writeBaroSourceMode(int mode) async => writtenModes.add(mode);

  @override
  Future<void> writePhoneAltitudeSample(double altitudeM, int seq) async {
    writtenSamples.add([altitudeM, seq]);
  }

  void dispose() {
    _controller.close();
    _connectionController.close();
  }
}

Future<void> _settle() async {
  await Future<void>.delayed(Duration.zero);
  await Future<void>.delayed(Duration.zero);
}

void main() {
  test('loadFromDevice reflects the mode read from the device', () async {
    final channel = FakeBaroSourceBleChannel()..modeToReturnOnRead = 1;
    final notifier = BaroSourceNotifier(
      bleChannel: channel,
      phoneBaroService: const PhoneBaroService(
          barometerEventStreamFactory: _emptyBarometerStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    await notifier.loadFromDevice();
    expect(notifier.state.mode, AltitudeSourceMode.phoneForced);
  });

  test('setMode writes the mode byte to the device', () async {
    final channel = FakeBaroSourceBleChannel();
    final notifier = BaroSourceNotifier(
      bleChannel: channel,
      phoneBaroService: const PhoneBaroService(
          barometerEventStreamFactory: _emptyBarometerStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    await notifier.setMode(AltitudeSourceMode.phoneForced);
    expect(channel.writtenModes, [1]);
    expect(notifier.state.mode, AltitudeSourceMode.phoneForced);
  });

  test('a device-side mode change (Notify) updates state', () async {
    final channel = FakeBaroSourceBleChannel();
    final notifier = BaroSourceNotifier(
      bleChannel: channel,
      phoneBaroService: const PhoneBaroService(
          barometerEventStreamFactory: _emptyBarometerStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    channel._controller.add(1);
    await Future<void>.delayed(Duration.zero);
    expect(notifier.state.mode, AltitudeSourceMode.phoneForced);
  });

  test('connecting starts streaming altitude to the device', () async {
    final channel = FakeBaroSourceBleChannel();
    final events = StreamController<BarometerEvent>.broadcast();
    final notifier = BaroSourceNotifier(
      bleChannel: channel,
      phoneBaroService: PhoneBaroService(
          barometerEventStreamFactory: () => events.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      events.close();
    });

    channel.setConnected(true);
    await _settle();
    // 1013.25 hPa is sea level -- altitude should come out ~0.
    events.add(BarometerEvent(1013.25, DateTime.now()));
    await _settle();

    expect(channel.writtenSamples, isNotEmpty);
    expect(channel.writtenSamples.first[0], closeTo(0, 1));
    expect(channel.writtenSamples.first[1], 1); // seq starts at 1
  });

  test('disconnecting stops streaming altitude', () async {
    final channel = FakeBaroSourceBleChannel();
    final events = StreamController<BarometerEvent>.broadcast();
    final notifier = BaroSourceNotifier(
      bleChannel: channel,
      phoneBaroService: PhoneBaroService(
          barometerEventStreamFactory: () => events.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      events.close();
    });

    channel.setConnected(true);
    await _settle();
    events.add(BarometerEvent(1000, DateTime.now()));
    await _settle();
    expect(channel.writtenSamples.length, 1);

    channel.setConnected(false);
    await _settle();
    events.add(BarometerEvent(990, DateTime.now()));
    await _settle();

    expect(channel.writtenSamples.length, 1,
        reason: 'no writes after the BLE link dropped');
  });
}

Stream<BarometerEvent> _emptyBarometerStream() => const Stream.empty();
