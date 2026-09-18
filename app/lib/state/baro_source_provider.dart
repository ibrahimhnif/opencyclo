import 'dart:async';
import 'package:flutter/foundation.dart' show debugPrint;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/ble/ble_service.dart';
import '../core/ble/ble_protocol.dart';
import '../core/location/phone_baro_service.dart';

/// The subset of BleService that BaroSourceNotifier needs -- lets tests
/// inject a fake without mocking the whole flutter_blue_plus-backed class.
abstract class BaroSourceBleChannel {
  Stream<int> get baroSourceModeStream;
  Stream<bool> get isConnectedStream;
  Future<int?> readBaroSourceMode();
  Future<void> writeBaroSourceMode(int mode);
  Future<void> writePhoneAltitudeSample(double altitudeM, int seq);
}

enum AltitudeSourceMode { hardware, phoneForced }

AltitudeSourceMode altitudeSourceModeFromByte(int b) =>
    b == BleProtocol.altitudeSourcePhoneForced
        ? AltitudeSourceMode.phoneForced
        : AltitudeSourceMode.hardware;

int altitudeSourceModeToByte(AltitudeSourceMode m) =>
    m == AltitudeSourceMode.phoneForced
        ? BleProtocol.altitudeSourcePhoneForced
        : BleProtocol.altitudeSourceHardware;

class BaroSourceState {
  final AltitudeSourceMode mode;
  final bool syncing;
  const BaroSourceState({
    this.mode = AltitudeSourceMode.hardware,
    this.syncing = false,
  });

  BaroSourceState copyWith({AltitudeSourceMode? mode, bool? syncing}) =>
      BaroSourceState(
        mode: mode ?? this.mode,
        syncing: syncing ?? this.syncing,
      );
}

/// Mirrors GpsSourceNotifier, minus the foreground-service dance: unlike
/// location, reading the barometer needs no runtime permission and Android
/// does not kill a backgrounded sensor stream the way it kills GPS.
class BaroSourceNotifier extends StateNotifier<BaroSourceState> {
  final BaroSourceBleChannel _bleChannel;
  final PhoneBaroService _phoneBaroService;
  StreamSubscription<PhoneBaroSample>? _sampleSub;
  StreamSubscription<int>? _modeSub;
  StreamSubscription<bool>? _connectionSub;
  int _seq = 0;

  BaroSourceNotifier({
    BaroSourceBleChannel? bleChannel,
    PhoneBaroService? phoneBaroService,
  })  : _bleChannel = bleChannel ?? BleService.instance,
        _phoneBaroService = phoneBaroService ?? const PhoneBaroService(),
        super(const BaroSourceState()) {
    _modeSub = _bleChannel.baroSourceModeStream.listen((byte) {
      state = state.copyWith(mode: altitudeSourceModeFromByte(byte));
    });
    _connectionSub = _bleChannel.isConnectedStream.listen((connected) {
      connected ? _startStreaming() : _stopStreaming();
    });
  }

  Future<void> loadFromDevice() async {
    final byte = await _bleChannel.readBaroSourceMode();
    if (byte != null && mounted) {
      state = state.copyWith(mode: altitudeSourceModeFromByte(byte));
    }
  }

  /// Same rationale as GpsSourceNotifier.setMode: streaming follows the BLE
  /// link, not the selected mode, since hardware mode can fall back to the
  /// phone at any moment.
  Future<void> setMode(AltitudeSourceMode mode) async {
    state = state.copyWith(mode: mode, syncing: true);
    await _bleChannel.writeBaroSourceMode(altitudeSourceModeToByte(mode));
    if (mounted) state = state.copyWith(syncing: false);
  }

  void _startStreaming() {
    if (_sampleSub != null) return;
    _sampleSub = _phoneBaroService.samples().listen(
      (sample) {
        _seq = (_seq + 1) & 0xFF;
        _bleChannel.writePhoneAltitudeSample(sample.altitudeM, _seq);
      },
      onError: (Object error) {
        debugPrint('[BARO SOURCE] sample stream error: $error');
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

final baroSourceProvider =
    StateNotifierProvider<BaroSourceNotifier, BaroSourceState>((ref) {
  return BaroSourceNotifier();
});
