import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../core/ble/ble_service.dart';

enum DeviceConnectionStatus { disconnected, connecting, connected }

class BleState {
  final bool isScanning;
  final List<ScanResult> scanResults;
  final BluetoothDevice? connectedDevice;
  final DeviceConnectionStatus status;
  final String errorMessage;

  const BleState({
    this.isScanning = false,
    this.scanResults = const [],
    this.connectedDevice,
    this.status = DeviceConnectionStatus.disconnected,
    this.errorMessage = '',
  });

  BleState copyWith({
    bool? isScanning,
    List<ScanResult>? scanResults,
    BluetoothDevice? connectedDevice,
    DeviceConnectionStatus? status,
    String? errorMessage,
  }) {
    return BleState(
      isScanning: isScanning ?? this.isScanning,
      scanResults: scanResults ?? this.scanResults,
      connectedDevice: connectedDevice ?? this.connectedDevice,
      status: status ?? this.status,
      errorMessage: errorMessage ?? this.errorMessage,
    );
  }
}

class BleNotifier extends StateNotifier<BleState> {
  BleNotifier() : super(const BleState()) {
    FlutterBluePlus.isScanning.listen((scanning) {
      state = state.copyWith(isScanning: scanning);
    });

    FlutterBluePlus.scanResults.listen((results) {
      state = state.copyWith(scanResults: results);
    });

    BleService.instance.connectionStateStream.listen((connState) {
      if (connState == BluetoothConnectionState.connected) {
        state = state.copyWith(status: DeviceConnectionStatus.connected);
      } else if (connState == BluetoothConnectionState.disconnected) {
        state = state.copyWith(status: DeviceConnectionStatus.disconnected, connectedDevice: null);
      }
    });
  }

  Future<void> startScan() async {
    state = state.copyWith(errorMessage: '');
    try {
      await BleService.instance.startScan();
    } catch (e) {
      state = state.copyWith(errorMessage: e.toString());
    }
  }

  Future<void> stopScan() async {
    await BleService.instance.stopScan();
  }

  Future<void> connect(BluetoothDevice device) async {
    state = state.copyWith(status: DeviceConnectionStatus.connecting, connectedDevice: device);
    final success = await BleService.instance.connect(device);
    if (!success) {
      state = state.copyWith(status: DeviceConnectionStatus.disconnected, errorMessage: 'Failed to connect');
    }
  }

  Future<void> disconnect() async {
    await BleService.instance.disconnect();
    state = state.copyWith(status: DeviceConnectionStatus.disconnected, connectedDevice: null);
  }
}

final bleProvider = StateNotifierProvider<BleNotifier, BleState>((ref) {
  return BleNotifier();
});
