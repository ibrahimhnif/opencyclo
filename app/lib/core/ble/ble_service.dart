import 'dart:async';
import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'ble_protocol.dart';
import '../models/telemetry_model.dart';
import '../models/layout_config_model.dart';

class BleService {
  static final BleService instance = BleService._internal();
  BleService._internal();

  BluetoothDevice? connectedDevice;
  BluetoothCharacteristic? _layoutChar;
  BluetoothCharacteristic? _commandChar;
  BluetoothCharacteristic? _otaControlChar;
  BluetoothCharacteristic? _otaDataChar;

  final _telemetryController = StreamController<TelemetryModel>.broadcast();
  Stream<TelemetryModel> get telemetryStream => _telemetryController.stream;

  final _connectionStateController = StreamController<BluetoothConnectionState>.broadcast();
  Stream<BluetoothConnectionState> get connectionStateStream => _connectionStateController.stream;

  Future<void> startScan() async {
    // Scan without withServices filter for reliable CoreBluetooth discovery on macOS / iOS
    await FlutterBluePlus.startScan(
      timeout: const Duration(seconds: 10),
    );
  }

  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
  }

  Future<bool> connect(BluetoothDevice device) async {
    try {
      connectedDevice = device;
      await device.connect(timeout: const Duration(seconds: 15), autoConnect: false);

      // Listen to connection state
      device.connectionState.listen((state) {
        _connectionStateController.add(state);
      });

      // Request MTU 512 for fast throughput
      await device.requestMtu(512);

      // Discover Services
      final services = await device.discoverServices();
      for (final service in services) {
        final sUuid = service.uuid.toString().toLowerCase();
        if (sUuid.contains("1900")) {
          for (final char in service.characteristics) {
            final uuid = char.uuid.toString().toLowerCase();
            if (uuid.contains("1901")) {
              _layoutChar = char;
            } else if (uuid.contains("1902")) {
              await _subscribeTelemetry(char);
            } else if (uuid.contains("1903")) {
              _commandChar = char;
            }
          }
        } else if (sUuid.contains("1910")) {
          for (final char in service.characteristics) {
            final uuid = char.uuid.toString().toLowerCase();
            if (uuid.contains("1911")) {
              _otaControlChar = char;
            } else if (uuid.contains("1912")) {
              _otaDataChar = char;
            }
          }
        }
      }

      return true;
    } catch (e) {
      debugPrint("[BLE ERROR] Connection failed: $e");
      return false;
    }
  }

  Future<void> disconnect() async {
    if (connectedDevice != null) {
      await connectedDevice!.disconnect();
      connectedDevice = null;
      _layoutChar = null;
      _commandChar = null;
      _otaControlChar = null;
      _otaDataChar = null;
    }
  }

  Future<void> _subscribeTelemetry(BluetoothCharacteristic char) async {
    await char.setNotifyValue(true);
    char.lastValueStream.listen((value) {
      if (value.isNotEmpty) {
        final model = TelemetryModel.fromBytes(value);
        _telemetryController.add(model);
      }
    });
  }

  // --- LAYOUT CONFIGURATION SYNC ---
  Future<UiConfigModel?> fetchLayoutConfig() async {
    if (_layoutChar == null) return null;
    try {
      final value = await _layoutChar!.read();
      final jsonStr = utf8.decode(value);
      return UiConfigModel.fromJsonString(jsonStr);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to fetch layout: $e");
      return null;
    }
  }

  Future<bool> sendLayoutConfig(UiConfigModel config) async {
    if (_layoutChar == null) return false;
    try {
      final jsonStr = config.toJsonString();
      final bytes = utf8.encode(jsonStr);
      await _layoutChar!.write(bytes, withoutResponse: false);
      return true;
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write layout: $e");
      return false;
    }
  }

  // --- DEVICE COMMANDS ---
  Future<void> sendCommand(int cmd) async {
    if (_commandChar == null) return;
    try {
      await _commandChar!.write([cmd], withoutResponse: false);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to send command: $e");
    }
  }

  // --- WIRELESS OTA FIRMWARE FLASHER ---
  Stream<double> flashFirmware(Uint8List firmwareBytes) async* {
    if (_otaControlChar == null || _otaDataChar == null) {
      throw Exception("OTA GATT characteristics not found on connected device.");
    }

    final int totalBytes = firmwareBytes.length;
    yield 0.0;

    // Step 1: Send Begin Command (0x01 + 4-byte size)
    final beginPayload = ByteData(5);
    beginPayload.setUint8(0, BleProtocol.otaCmdBegin);
    beginPayload.setUint32(1, totalBytes, Endian.little);

    await _otaControlChar!.write(beginPayload.buffer.asUint8List(), withoutResponse: false);
    await Future.delayed(const Duration(milliseconds: 200));

    // Step 2: Stream binary chunks (480 bytes per packet)
    const int chunkSize = 480;
    int offset = 0;

    while (offset < totalBytes) {
      final int end = (offset + chunkSize < totalBytes) ? offset + chunkSize : totalBytes;
      final chunk = firmwareBytes.sublist(offset, end);

      await _otaDataChar!.write(chunk, withoutResponse: true);
      offset = end;

      final progress = offset / totalBytes;
      yield progress;

      // Small throttle to avoid buffer congestion
      await Future.delayed(const Duration(milliseconds: 8));
    }

    // Step 3: Send End Command (0x02) to verify & reboot
    await _otaControlChar!.write([BleProtocol.otaCmdEnd], withoutResponse: false);
    yield 1.0;
  }
}
