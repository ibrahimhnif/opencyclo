import 'dart:async';
import 'package:flutter_test/flutter_test.dart';
import 'package:geolocator/geolocator.dart';
import 'package:opencyclo_app/core/location/phone_gps_service.dart';
import 'package:opencyclo_app/state/gps_source_provider.dart';

class FakeGpsSourceBleChannel implements GpsSourceBleChannel {
  int? modeToReturnOnRead;
  final List<int> writtenModes = [];
  final List<List<num>> writtenSamples = [];
  final _controller = StreamController<int>.broadcast();

  @override
  Stream<int> get gpsSourceModeStream => _controller.stream;

  @override
  Future<int?> readGpsSourceMode() async => modeToReturnOnRead;

  @override
  Future<void> writeGpsSourceMode(int mode) async => writtenModes.add(mode);

  @override
  Future<void> writePhoneGpsSample(
      double lat, double lon, double accuracyM, int seq) async {
    writtenSamples.add([lat, lon, accuracyM, seq]);
  }

  void dispose() => _controller.close();
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
}

Stream<Position> _emptyPositionStream() => const Stream.empty();
