import '../core/ble/ble_protocol.dart';
import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../core/ble/ble_service.dart';
import '../core/ble/ble_access.dart';
export '../core/ble/ble_access.dart' show BleAccess;

enum DeviceConnectionStatus { disconnected, connecting, connected }

class BleState {
  final bool isScanning;
  final List<ScanResult> scanResults;
  final BluetoothDevice? connectedDevice;
  final DeviceConnectionStatus status;
  final String errorMessage;
  final BleAccess access;
  final bool checkingAccess;

  const BleState({
    this.isScanning = false,
    this.scanResults = const [],
    this.connectedDevice,
    this.status = DeviceConnectionStatus.disconnected,
    this.errorMessage = '',
    this.access = BleAccess.unknown,
    this.checkingAccess = false,
  });

  BleState copyWith({
    bool? isScanning,
    List<ScanResult>? scanResults,
    BluetoothDevice? connectedDevice,
    DeviceConnectionStatus? status,
    String? errorMessage,
    BleAccess? access,
    bool? checkingAccess,
    bool clearDevice = false,
  }) {
    return BleState(
      isScanning: isScanning ?? this.isScanning,
      scanResults: scanResults ?? this.scanResults,
      connectedDevice:
          clearDevice ? null : connectedDevice ?? this.connectedDevice,
      status: status ?? this.status,
      errorMessage: errorMessage ?? this.errorMessage,
      access: access ?? this.access,
      checkingAccess: checkingAccess ?? this.checkingAccess,
    );
  }
}

class BleNotifier extends StateNotifier<BleState> {
  final BleAccessGate _accessGate;
  final List<StreamSubscription<dynamic>> _subscriptions = [];
  BleNotifier({BleAccessGate? accessGate})
      : _accessGate = accessGate ?? BleAccessGate(),
        super(const BleState()) {
    _subscriptions.add(FlutterBluePlus.isScanning.listen((scanning) {
      state = state.copyWith(isScanning: scanning);
    }));

    _subscriptions.add(FlutterBluePlus.scanResults.listen((results) {
      state = state.copyWith(scanResults: results);
    }));

    _subscriptions
        .add(BleService.instance.connectionStateStream.listen((connState) {
      if (connState == BluetoothConnectionState.connected) {
        state = state.copyWith(status: DeviceConnectionStatus.connected);
      } else if (connState == BluetoothConnectionState.disconnected) {
        state = state.copyWith(
            status: DeviceConnectionStatus.disconnected, clearDevice: true);
      }
    }));
  }

  Future<bool> checkAccess({bool request = false}) async {
    if (state.checkingAccess) return false;
    state = state.copyWith(checkingAccess: true, errorMessage: '');
    try {
      final access = await _accessGate.check(request: request);
      if (!mounted) return false;
      state = state.copyWith(
          access: access,
          checkingAccess: false,
          scanResults: access == BleAccess.ready ? null : []);
      return access == BleAccess.ready;
    } catch (error) {
      if (mounted) {
        state = state.copyWith(
            access: BleAccess.unknown,
            checkingAccess: false,
            errorMessage: 'Bluetooth check failed. Please retry.');
      }
      return false;
    }
  }

  Future<void> resolveAccess() async {
    if (state.checkingAccess) return;
    if (state.access == BleAccess.blocked ||
        state.access == BleAccess.locationOff) {
      try {
        await _accessGate.openSettings();
      } catch (_) {
        if (mounted) {
          state = state.copyWith(
              errorMessage: 'Open app permissions in Android Settings.');
        }
      }
    } else {
      await checkAccess(request: true);
    }
  }

  Future<void> startScan() async {
    if (!await checkAccess(request: true) || !mounted) return;
    state = state.copyWith(errorMessage: '');
    try {
      await BleService.instance.startScan();
    } catch (e) {
      if (mounted) state = state.copyWith(errorMessage: e.toString());
    }
  }

  Future<void> stopScan() async {
    await BleService.instance.stopScan();
  }

  Future<void> connect(BluetoothDevice device) async {
    if (state.status == DeviceConnectionStatus.connecting) return;
    if (!await checkAccess(request: true) || !mounted) return;
    state = state.copyWith(
        status: DeviceConnectionStatus.connecting, connectedDevice: device);
    final success = await BleService.instance.connect(device);
    if (mounted && !success) {
      state = state.copyWith(
          status: DeviceConnectionStatus.disconnected,
          clearDevice: true,
          errorMessage: 'Failed to connect');
    }
  }

  /// Asks the device to save its current screen to /screenshots on its SD
  /// card. Fire-and-forget: the device shows the saved file name itself.
  Future<void> requestScreenshot() =>
      BleService.instance.sendCommand(BleProtocol.cmdScreenshot);

  Future<void> disconnect() async {
    await BleService.instance.disconnect();
    if (!mounted) return;
    state = state.copyWith(
        status: DeviceConnectionStatus.disconnected, clearDevice: true);
  }

  @override
  void dispose() {
    for (final subscription in _subscriptions) {
      subscription.cancel();
    }
    super.dispose();
  }
}

final bleProvider = StateNotifierProvider<BleNotifier, BleState>((ref) {
  return BleNotifier();
});
