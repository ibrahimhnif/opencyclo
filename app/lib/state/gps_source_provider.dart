import 'dart:async';
import 'dart:io' show Platform;
import 'package:flutter/foundation.dart' show debugPrint, kIsWeb;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import '../core/ble/ble_service.dart';
import '../core/ble/ble_protocol.dart';
import '../core/location/phone_gps_service.dart';

/// The subset of BleService that GpsSourceNotifier needs -- lets tests
/// inject a fake without mocking the whole flutter_blue_plus-backed class.
abstract class GpsSourceBleChannel {
  Stream<int> get gpsSourceModeStream;

  /// true while a device is connected, false on disconnect/disconnecting.
  Stream<bool> get isConnectedStream;
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

  /// Non-null when phone GPS could not be supplied (permission refused,
  /// location services off, foreground service refused, stream error).
  final String? error;
  const GpsSourceState({
    this.mode = GpsSourceMode.hardware,
    this.syncing = false,
    this.error,
  });

  GpsSourceState copyWith({
    GpsSourceMode? mode,
    bool? syncing,
    String? error,
    bool clearError = false,
  }) =>
      GpsSourceState(
        mode: mode ?? this.mode,
        syncing: syncing ?? this.syncing,
        error: clearError ? null : (error ?? this.error),
      );
}

class GpsSourceNotifier extends StateNotifier<GpsSourceState> {
  final GpsSourceBleChannel _bleChannel;
  final PhoneGpsService _phoneGpsService;
  StreamSubscription<PhoneGpsSample>? _positionSub;
  StreamSubscription<int>? _modeSub;
  StreamSubscription<bool>? _connectionSub;
  /// Serialises start/stop so a connect/disconnect flap cannot interleave a
  /// half-finished start (permission + foreground service both await) with the
  /// stop that should have cancelled it.
  Future<void> _streamWork = Future<void>.value();
  int _seq = 0;

  GpsSourceNotifier({
    GpsSourceBleChannel? bleChannel,
    PhoneGpsService? phoneGpsService,
  })  : _bleChannel = bleChannel ?? BleService.instance,
        _phoneGpsService = phoneGpsService ?? const PhoneGpsService(),
        super(const GpsSourceState()) {
    // Mode changes made on the device's own touchscreen: display only. Which
    // source the firmware prefers does not decide whether we stream -- see
    // _onConnectionChanged.
    _modeSub = _bleChannel.gpsSourceModeStream.listen((byte) {
      state = state.copyWith(mode: gpsSourceModeFromByte(byte));
    });
    _connectionSub = _bleChannel.isConnectedStream.listen(_onConnectionChanged);
  }

  Future<void> loadFromDevice() async {
    final byte = await _bleChannel.readGpsSourceMode();
    if (byte != null && mounted) {
      state = state.copyWith(mode: gpsSourceModeFromByte(byte));
    }
  }

  /// Writes which source the firmware should prefer. Deliberately does *not*
  /// start or stop position streaming: hardware mode auto-falls back to the
  /// phone, which only works if a recent phone sample is already queued on the
  /// device, so the phone streams in both modes while connected.
  Future<void> setMode(GpsSourceMode mode) async {
    state = state.copyWith(mode: mode, syncing: true);
    await _bleChannel.writeGpsSourceMode(gpsSourceModeToByte(mode));
    if (mounted) state = state.copyWith(syncing: false, clearError: true);
  }

  /// Single place where streaming starts and stops: streaming follows the BLE
  /// link, not the selected mode.
  Future<void> _onConnectionChanged(bool connected) {
    _streamWork = _streamWork
        .then((_) =>
            connected ? _startStreamingPosition() : _stopStreamingPosition())
        .catchError((Object e) {
      // Never let one failure poison the chain -- the next connect must run.
      debugPrint('[GPS SOURCE] stream start/stop failed: $e');
    });
    return _streamWork;
  }

  Future<void> _startStreamingPosition() async {
    if (_positionSub != null) return;

    bool permitted;
    try {
      permitted = await _phoneGpsService.ensurePermission();
    } catch (e) {
      debugPrint('[GPS SOURCE] permission check failed: $e');
      permitted = false;
    }
    if (!permitted) {
      if (mounted) {
        state = state.copyWith(
            error: 'Location permission is required to use phone GPS.');
      }
      return;
    }

    if (!await _startForegroundService()) return;
    if (!mounted) return;

    _positionSub = _phoneGpsService.positions().listen(
      (sample) {
        _seq = (_seq + 1) & 0xFF;
        _bleChannel.writePhoneGpsSample(
            sample.latitude, sample.longitude, sample.accuracyM, _seq);
      },
      onError: (Object error) {
        debugPrint('[GPS SOURCE] position stream error: $error');
        if (mounted) state = state.copyWith(error: error.toString());
      },
    );
    state = state.copyWith(clearError: true);
  }

  /// Android kills a backgrounded app's location stream without a foreground
  /// service. Returns false when the service was requested but refused, in
  /// which case streaming is not started at all.
  Future<bool> _startForegroundService() async {
    if (!_foregroundServiceSupported) return true;
    try {
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

      // Android 13+ hides the service notification without this. Best effort:
      // the service itself still runs if the user says no.
      if (await FlutterForegroundTask.checkNotificationPermission() !=
          NotificationPermission.granted) {
        final granted =
            await FlutterForegroundTask.requestNotificationPermission();
        if (granted != NotificationPermission.granted) {
          debugPrint(
              '[GPS SOURCE] notification permission $granted; the background '
              'location service notification may be hidden');
        }
      }

      final result = await FlutterForegroundTask.startService(
        notificationTitle: 'opencyclo',
        notificationText: 'using phone GPS as location source',
      );
      if (result is ServiceRequestFailure) {
        debugPrint('[GPS SOURCE] startService failed: ${result.error}');
        if (mounted) {
          state = state.copyWith(
              error: 'Failed to start background location service');
        }
        return false;
      }
      return true;
    } catch (e) {
      debugPrint('[GPS SOURCE] startService threw: $e');
      if (mounted) {
        state = state.copyWith(
            error: 'Failed to start background location service');
      }
      return false;
    }
  }

  Future<void> _stopStreamingPosition() async {
    await _positionSub?.cancel();
    _positionSub = null;
    if (!_foregroundServiceSupported) return;
    try {
      await FlutterForegroundTask.stopService();
    } catch (e) {
      debugPrint('[GPS SOURCE] stopService threw: $e');
    }
  }

  /// flutter_foreground_task only has a platform implementation on Android and
  /// iOS; on desktop (and under `flutter test`) there is no channel to call.
  static bool get _foregroundServiceSupported =>
      !kIsWeb && (Platform.isAndroid || Platform.isIOS);

  @override
  void dispose() {
    _modeSub?.cancel();
    _connectionSub?.cancel();
    // Fire and forget: also tears down the foreground service, which would
    // otherwise outlive the notifier holding its notification up.
    unawaited(_stopStreamingPosition().catchError((Object e) {
      debugPrint('[GPS SOURCE] stop on dispose failed: $e');
    }));
    super.dispose();
  }
}

final gpsSourceProvider =
    StateNotifierProvider<GpsSourceNotifier, GpsSourceState>((ref) {
  return GpsSourceNotifier();
});
