import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import '../core/ble/ble_service.dart';
import '../core/ble/ble_protocol.dart';
import '../core/location/phone_gps_service.dart';

/// The subset of BleService that GpsSourceNotifier needs -- lets tests
/// inject a fake without mocking the whole flutter_blue_plus-backed class.
abstract class GpsSourceBleChannel {
  Stream<int> get gpsSourceModeStream;
  Future<int?> readGpsSourceMode();
  Future<void> writeGpsSourceMode(int mode);
  Future<void> writePhoneGpsSample(
      double lat, double lon, double accuracyM, int seq);
}

enum GpsSourceMode { hardware, phoneForced }

GpsSourceMode gpsSourceModeFromByte(int b) =>
    b == BleProtocol.gpsSourcePhoneForced
        ? GpsSourceMode.phoneForced
        : GpsSourceMode.hardware;

int gpsSourceModeToByte(GpsSourceMode m) =>
    m == GpsSourceMode.phoneForced
        ? BleProtocol.gpsSourcePhoneForced
        : BleProtocol.gpsSourceHardware;

class GpsSourceState {
  final GpsSourceMode mode;
  final bool syncing;
  const GpsSourceState({
    this.mode = GpsSourceMode.hardware,
    this.syncing = false,
  });

  GpsSourceState copyWith({GpsSourceMode? mode, bool? syncing}) =>
      GpsSourceState(mode: mode ?? this.mode, syncing: syncing ?? this.syncing);
}

class GpsSourceNotifier extends StateNotifier<GpsSourceState> {
  final GpsSourceBleChannel _bleChannel;
  final PhoneGpsService _phoneGpsService;
  StreamSubscription<PhoneGpsSample>? _positionSub;
  StreamSubscription<int>? _modeSub;
  int _seq = 0;

  GpsSourceNotifier({
    GpsSourceBleChannel? bleChannel,
    PhoneGpsService? phoneGpsService,
  })  : _bleChannel = bleChannel ?? BleService.instance,
        _phoneGpsService = phoneGpsService ?? const PhoneGpsService(),
        super(const GpsSourceState()) {
    _modeSub = _bleChannel.gpsSourceModeStream.listen((byte) {
      state = state.copyWith(mode: gpsSourceModeFromByte(byte));
    });
  }

  Future<void> loadFromDevice() async {
    final byte = await _bleChannel.readGpsSourceMode();
    if (byte != null && mounted) {
      state = state.copyWith(mode: gpsSourceModeFromByte(byte));
    }
  }

  Future<void> setMode(GpsSourceMode mode) async {
    state = state.copyWith(mode: mode, syncing: true);
    await _bleChannel.writeGpsSourceMode(gpsSourceModeToByte(mode));
    if (mode == GpsSourceMode.phoneForced) {
      await _startStreamingPosition();
    } else {
      await _stopStreamingPosition();
    }
    if (mounted) state = state.copyWith(syncing: false);
  }

  Future<void> _startStreamingPosition() async {
    if (_positionSub != null) return;
    FlutterForegroundTask.init(
      androidNotificationOptions: AndroidNotificationOptions(
        channelId: 'gps_source_channel',
        channelName: 'Phone GPS Source',
        channelDescription:
            'Keeps phone GPS active while used as the opencyclo GPS source.',
      ),
      iosNotificationOptions: const IOSNotificationOptions(),
      foregroundTaskOptions: ForegroundTaskOptions(
        eventAction: ForegroundTaskEventAction.nothing(),
        autoRunOnBoot: false,
        allowWakeLock: true,
      ),
    );
    await FlutterForegroundTask.startService(
      notificationTitle: 'opencyclo',
      notificationText: 'using phone GPS as location source',
    );
    _positionSub = _phoneGpsService.positions().listen((sample) {
      _seq = (_seq + 1) & 0xFF;
      _bleChannel.writePhoneGpsSample(
          sample.latitude, sample.longitude, sample.accuracyM, _seq);
    });
  }

  Future<void> _stopStreamingPosition() async {
    await _positionSub?.cancel();
    _positionSub = null;
    await FlutterForegroundTask.stopService();
  }

  @override
  void dispose() {
    _positionSub?.cancel();
    _modeSub?.cancel();
    super.dispose();
  }
}

final gpsSourceProvider =
    StateNotifierProvider<GpsSourceNotifier, GpsSourceState>((ref) {
  return GpsSourceNotifier();
});
