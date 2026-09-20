import 'dart:async';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_compass/flutter_compass.dart';
import 'package:opencyclo/core/location/phone_compass_service.dart';
import 'package:opencyclo/state/compass_source_provider.dart';

class FakeCompassSourceBleChannel implements CompassSourceBleChannel {
  int? modeToReturnOnRead;
  final List<int> writtenModes = [];
  final List<List<num>> writtenSamples = [];
  final _controller = StreamController<int>.broadcast();
  final _connectionController = StreamController<bool>.broadcast();

  @override
  Stream<int> get compassSourceModeStream => _controller.stream;

  @override
  Stream<bool> get isConnectedStream => _connectionController.stream;

  void setConnected(bool connected) => _connectionController.add(connected);

  @override
  Future<int?> readCompassSourceMode() async => modeToReturnOnRead;

  @override
  Future<void> writeCompassSourceMode(int mode) async =>
      writtenModes.add(mode);

  @override
  Future<void> writePhoneHeadingSample(
      double headingDeg, int accuracy, int seq) async {
    writtenSamples.add([headingDeg, accuracy, seq]);
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
    final channel = FakeCompassSourceBleChannel()..modeToReturnOnRead = 1;
    final notifier = CompassSourceNotifier(
      bleChannel: channel,
      phoneCompassService:
          const PhoneCompassService(eventsFactory: _emptyCompassStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    await notifier.loadFromDevice();
    expect(notifier.state.mode, HeadingSourceMode.phoneForced);
  });

  test('setMode writes the mode byte to the device', () async {
    final channel = FakeCompassSourceBleChannel();
    final notifier = CompassSourceNotifier(
      bleChannel: channel,
      phoneCompassService:
          const PhoneCompassService(eventsFactory: _emptyCompassStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    await notifier.setMode(HeadingSourceMode.phoneForced);
    expect(channel.writtenModes, [1]);
    expect(notifier.state.mode, HeadingSourceMode.phoneForced);
  });

  test('connecting starts streaming heading to the device', () async {
    final channel = FakeCompassSourceBleChannel();
    final events = StreamController<CompassEvent>.broadcast();
    final notifier = CompassSourceNotifier(
      bleChannel: channel,
      phoneCompassService:
          PhoneCompassService(eventsFactory: () => events.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      events.close();
    });

    channel.setConnected(true);
    await _settle();
    events.add(CompassEvent.fromList([90.0, 0.0, 10.0]));
    await _settle();

    expect(channel.writtenSamples, isNotEmpty);
    expect(channel.writtenSamples.first[0], 90.0);
    expect(channel.writtenSamples.first[1], 2); // 10deg error -> high accuracy
    expect(channel.writtenSamples.first[2], 1); // seq starts at 1
    expect(notifier.state.error, isNull);
  });

  test('a null heading sample is skipped', () async {
    final channel = FakeCompassSourceBleChannel();
    final events = StreamController<CompassEvent>.broadcast();
    final notifier = CompassSourceNotifier(
      bleChannel: channel,
      phoneCompassService:
          PhoneCompassService(eventsFactory: () => events.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      events.close();
    });

    channel.setConnected(true);
    await _settle();
    events.add(CompassEvent.fromList(null));
    await _settle();

    expect(channel.writtenSamples, isEmpty);
  });

  test('disconnecting stops streaming heading', () async {
    final channel = FakeCompassSourceBleChannel();
    final events = StreamController<CompassEvent>.broadcast();
    final notifier = CompassSourceNotifier(
      bleChannel: channel,
      phoneCompassService:
          PhoneCompassService(eventsFactory: () => events.stream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
      events.close();
    });

    channel.setConnected(true);
    await _settle();
    events.add(CompassEvent.fromList([10.0, 0.0, 10.0]));
    await _settle();
    expect(channel.writtenSamples.length, 1);

    channel.setConnected(false);
    await _settle();
    events.add(CompassEvent.fromList([20.0, 0.0, 10.0]));
    await _settle();

    expect(channel.writtenSamples.length, 1,
        reason: 'no writes after the BLE link dropped');
  });

  test('no compass hardware surfaces an error instead of throwing', () async {
    final channel = FakeCompassSourceBleChannel();
    final notifier = CompassSourceNotifier(
      bleChannel: channel,
      phoneCompassService:
          const PhoneCompassService(eventsFactory: _noCompassStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    channel.setConnected(true);
    await _settle();

    expect(notifier.state.error, contains('no compass sensor'));
  });
}

Stream<CompassEvent>? _emptyCompassStream() => const Stream.empty();
Stream<CompassEvent>? _noCompassStream() => null;
