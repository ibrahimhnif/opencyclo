import 'dart:async';
import 'package:flutter/foundation.dart' show debugPrint;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/ble/ble_service.dart';
import '../core/ble/ble_protocol.dart';
import '../core/location/phone_compass_service.dart';

/// The subset of BleService that CompassSourceNotifier needs -- lets tests
/// inject a fake without mocking the whole flutter_blue_plus-backed class.
abstract class CompassSourceBleChannel {
  Stream<int> get compassSourceModeStream;
  Stream<bool> get isConnectedStream;
  Future<int?> readCompassSourceMode();
  Future<void> writeCompassSourceMode(int mode);
  Future<void> writePhoneHeadingSample(double headingDeg, int accuracy, int seq);
}

enum HeadingSourceMode { hardware, phoneForced }

HeadingSourceMode headingSourceModeFromByte(int b) =>
    b == BleProtocol.headingSourcePhoneForced
        ? HeadingSourceMode.phoneForced
        : HeadingSourceMode.hardware;

int headingSourceModeToByte(HeadingSourceMode m) =>
    m == HeadingSourceMode.phoneForced
        ? BleProtocol.headingSourcePhoneForced
        : BleProtocol.headingSourceHardware;

class CompassSourceState {
  final HeadingSourceMode mode;
  final bool syncing;

  /// Non-null when the phone has no magnetometer at all -- distinct from a
  /// sample simply not having arrived yet.
  final String? error;
  const CompassSourceState({
    this.mode = HeadingSourceMode.hardware,
    this.syncing = false,
    this.error,
  });

  CompassSourceState copyWith(
          {HeadingSourceMode? mode, bool? syncing, String? error}) =>
      CompassSourceState(
        mode: mode ?? this.mode,
        syncing: syncing ?? this.syncing,
        error: error ?? this.error,
      );
}

/// Mirrors GpsSourceNotifier/BaroSourceNotifier: streaming follows the BLE
/// link, not the selected mode, and there is no runtime permission or
/// foreground-service requirement for reading a magnetometer.
class CompassSourceNotifier extends StateNotifier<CompassSourceState> {
  final CompassSourceBleChannel _bleChannel;
  final PhoneCompassService _phoneCompassService;
  StreamSubscription<PhoneCompassSample>? _sampleSub;
  StreamSubscription<int>? _modeSub;
  StreamSubscription<bool>? _connectionSub;
  int _seq = 0;

  CompassSourceNotifier({
    CompassSourceBleChannel? bleChannel,
    PhoneCompassService? phoneCompassService,
  })  : _bleChannel = bleChannel ?? BleService.instance,
        _phoneCompassService = phoneCompassService ?? const PhoneCompassService(),
        super(const CompassSourceState()) {
    _modeSub = _bleChannel.compassSourceModeStream.listen((byte) {
      state = state.copyWith(mode: headingSourceModeFromByte(byte));
    });
    _connectionSub = _bleChannel.isConnectedStream.listen((connected) {
      connected ? _startStreaming() : _stopStreaming();
    });
  }

  Future<void> loadFromDevice() async {
    final byte = await _bleChannel.readCompassSourceMode();
    if (byte != null && mounted) {
      state = state.copyWith(mode: headingSourceModeFromByte(byte));
    }
  }

  Future<void> setMode(HeadingSourceMode mode) async {
    state = state.copyWith(mode: mode, syncing: true);
    await _bleChannel.writeCompassSourceMode(headingSourceModeToByte(mode));
    if (mounted) state = state.copyWith(syncing: false);
  }

  void _startStreaming() {
    if (_sampleSub != null) return;
    if (!_phoneCompassService.isSupported) {
      state = state.copyWith(error: 'This phone has no compass sensor.');
      return;
    }
    _sampleSub = _phoneCompassService.samples().listen(
      (sample) {
        _seq = (_seq + 1) & 0xFF;
        _bleChannel.writePhoneHeadingSample(
            sample.headingDeg, sample.accuracy, _seq);
      },
      onError: (Object error) {
        debugPrint('[COMPASS SOURCE] sample stream error: $error');
      },
    );
  }

  void _stopStreaming() {
    _sampleSub?.cancel();
    _sampleSub = null;
  }

  @override
  void dispose() {
    _modeSub?.cancel();
    _connectionSub?.cancel();
    _stopStreaming();
    super.dispose();
  }
}

final compassSourceProvider =
    StateNotifierProvider<CompassSourceNotifier, CompassSourceState>((ref) {
  return CompassSourceNotifier();
});
