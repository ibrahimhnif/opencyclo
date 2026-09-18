import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'ble_protocol.dart';
import '../models/telemetry_model.dart';
import '../models/layout_config_model.dart';
import '../models/route_model.dart';
import 'route_transfer.dart';
import 'ride_download.dart';
import 'ride_download_fast.dart';
import 'gps_assistance.dart';
import 'gps_registration.dart';
import 'gps_credentials.dart';
import 'gps_cache.dart';
import '../../state/gps_source_provider.dart' show GpsSourceBleChannel;

class BleService implements GpsSourceBleChannel {
  static final BleService instance = BleService._internal();
  BleService._internal();

  BluetoothDevice? connectedDevice;
  BluetoothCharacteristic? _layoutChar;
  BluetoothCharacteristic? _commandChar;
  BluetoothCharacteristic? _otaControlChar;
  BluetoothCharacteristic? _otaDataChar;
  BluetoothCharacteristic? _routeControlChar;
  BluetoothCharacteristic? _routeDataChar;
  BluetoothCharacteristic? _rideExportChar;
  BluetoothCharacteristic? _gpsAssistanceChar;
  BluetoothCharacteristic? _gpsIdentityChar;
  BluetoothCharacteristic? _gpsCacheChar;
  String? _credentialKey;
  final _gpsCredentials = const GpsCredentials();
  BluetoothCharacteristic? _phoneGpsChar;
  BluetoothCharacteristic? _gpsSourceModeChar;
  bool _routeBusy = false;
  bool _commandBusy = false;

  final _telemetryController = StreamController<TelemetryModel>.broadcast();
  Stream<TelemetryModel> get telemetryStream => _telemetryController.stream;

  final _gpsSourceModeController = StreamController<int>.broadcast();
  @override
  Stream<int> get gpsSourceModeStream => _gpsSourceModeController.stream;

  final _connectionStateController =
      StreamController<BluetoothConnectionState>.broadcast();
  Stream<BluetoothConnectionState> get connectionStateStream =>
      _connectionStateController.stream;

  /// Connection state as a plain bool for consumers that only care whether the
  /// link is up (the phone GPS stream follows this, in both GPS source modes).
  @override
  Stream<bool> get isConnectedStream => connectionStateStream
      .map((s) => s == BluetoothConnectionState.connected);

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
      _routeControlChar = null;
      _routeDataChar = null;
      _rideExportChar = null;
      _gpsAssistanceChar = null;
      _gpsIdentityChar = null;
      _gpsCacheChar = null;
      _credentialKey = null;
      _phoneGpsChar = null;
      _gpsSourceModeChar = null;
      await device.connect(
          timeout: const Duration(seconds: 15), autoConnect: false);

      // Listen to connection state
      device.connectionState.listen((state) {
        _connectionStateController.add(state);
        if (state == BluetoothConnectionState.disconnected) {
          _routeControlChar = null;
          _routeDataChar = null;
          _rideExportChar = null;
          _gpsAssistanceChar = null;
          _gpsIdentityChar = null;
          _gpsCacheChar = null;
          _credentialKey = null;
          _phoneGpsChar = null;
          _gpsSourceModeChar = null;
        }
      });

      // Request MTU 512 on Android only (iOS/macOS CoreBluetooth handles MTU automatically)
      if (!kIsWeb && Platform.isAndroid) {
        try {
          await device.requestMtu(512);
        } catch (e) {
          debugPrint("[BLE] requestMtu error (ignorable): $e");
        }
      }

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
            } else if (uuid.contains("1904")) {
              _routeControlChar = char;
            } else if (uuid.contains("1905")) {
              _routeDataChar = char;
            } else if (uuid.contains("1906")) {
              _rideExportChar = char;
            } else if (uuid.contains("1907")) {
              _gpsAssistanceChar = char;
            } else if (uuid.contains("1908")) {
              _gpsIdentityChar = char;
            } else if (uuid.contains("1909")) {
              _gpsCacheChar = char;
            } else if (uuid.contains("190a")) {
              _phoneGpsChar = char;
            } else if (uuid.contains("190b")) {
              _gpsSourceModeChar = char;
              await _subscribeGpsSourceMode(char);
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
    _gpsAssistanceChar = null;
    _gpsIdentityChar = null;
    _gpsCacheChar = null;
    _credentialKey = null;
    if (connectedDevice != null) {
      await connectedDevice!.disconnect();
      connectedDevice = null;
      _layoutChar = null;
      _commandChar = null;
      _otaControlChar = null;
      _otaDataChar = null;
      _phoneGpsChar = null;
      _gpsSourceModeChar = null;
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

  Future<void> _subscribeGpsSourceMode(BluetoothCharacteristic char) async {
    await char.setNotifyValue(true);
    char.lastValueStream.listen((value) {
      if (value.isNotEmpty) _gpsSourceModeController.add(value[0]);
    });
  }

  // --- LAYOUT CONFIGURATION SYNC ---
  Future<GpsIdentity> _readCurrentGpsIdentity() async {
    final c = _gpsIdentityChar;
    if (c == null) {
      throw StateError('Connect and update firmware for GPS identity');
    }
    final id = await readGpsIdentity((p) async {
      if (_gpsIdentityChar != c) throw StateError('Device disconnected');
      await c.write(p, withoutResponse: false);
    }, c.read);
    if (_gpsIdentityChar != c) throw StateError('Device disconnected');
    return id;
  }

  Future<String?> savedGpsChipcode({String? save, bool forget = false}) async {
    if (_routeBusy) throw StateError('Wait for current transfer');
    _routeBusy = true;
    try {
      final id = await _readCurrentGpsIdentity();
      if (forget) await _gpsCredentials.forget(id);
      if (save != null) await _gpsCredentials.save(id, save);
      final code = await _gpsCredentials.load(id);
      _credentialKey = code == null ? null : GpsCredentials.key(id);
      return code;
    } finally {
      _routeBusy = false;
    }
  }

  Future<GpsCacheStatus> gpsCacheStatus() async {
    final c = _gpsCacheChar;
    if (c == null) throw StateError('Update firmware for GPS cache');
    return GpsCacheStatus.parse(await c.read());
  }

  Future<void> syncGpsCache(
      String chipcode, void Function(String, double) update,
      {bool Function()? cancelled}) async {
    if (_routeBusy) throw StateError('Wait for current transfer');
    final c = _gpsCacheChar;
    if (c == null) throw StateError('Update firmware for GPS cache');
    _routeBusy = true;
    try {
      final id = await _readCurrentGpsIdentity();
      if (_credentialKey != null && _credentialKey != GpsCredentials.key(id)) {
        throw StateError('GPS receiver changed; load its Chipcode');
      }
      update('downloading 7-day predictions...', 0);
      final cache = await PredictiveCache.fetch(chipcode);
      await uploadGpsCache(cache, (p) async {
        if (_gpsCacheChar != c) throw StateError('Device disconnected');
        await c.write(p, withoutResponse: false);
      }, c.read, connectedDevice?.mtuNow ?? 23,
          (p) => update(p == 1 ? 'cache saved to SD' : 'saving cache...', p),
          cancelled: cancelled);
    } finally {
      _routeBusy = false;
    }
  }

  Future<String> registerGps(String token, void Function(String) update,
      {bool Function()? cancelled}) async {
    normalizeZtpToken(token);
    if (_routeBusy) throw StateError('Wait for the current transfer');
    final c = _gpsIdentityChar;
    if (c == null) {
      throw StateError('Connect and update firmware for GPS registration');
    }
    void check() {
      if ((cancelled?.call() ?? false) || _gpsIdentityChar != c) {
        throw StateError('Registration cancelled or device disconnected');
      }
    }

    _routeBusy = true;
    try {
      update('reading GPS identity...');
      final identity = await readGpsIdentity((p) async {
        if (_gpsIdentityChar != c) throw StateError('Device disconnected');
        await c.write(p, withoutResponse: false);
      }, c.read, cancelled: cancelled);
      check();
      update('registering with Thingstream...');
      final chipcode = await registerGpsIdentity(token, identity);
      check();
      await _gpsCredentials.save(identity, chipcode);
      _credentialKey = GpsCredentials.key(identity);
      return chipcode;
    } finally {
      _routeBusy = false;
    }
  }

  Future<void> syncGps(String chipcode, void Function(String, double) update,
      {bool Function()? cancelled}) async {
    if (_routeBusy) throw StateError('Wait for the current transfer');
    final characteristic = _gpsAssistanceChar;
    if (characteristic == null) {
      throw StateError('Connect and update firmware for GPS sync');
    }
    _routeBusy = true;
    try {
      update('downloading assistance...', 0);
      final id = await _readCurrentGpsIdentity();
      if (_credentialKey != null && _credentialKey != GpsCredentials.key(id)) {
        throw StateError('GPS receiver changed; load its Chipcode');
      }
      final data = await LiveAssistance.fetch(chipcode);
      await sendLiveAssistance(data, (bytes) async {
        if (_gpsAssistanceChar != characteristic) {
          throw StateError('Device disconnected');
        }
        await characteristic.write(bytes, withoutResponse: false);
      },
          characteristic.read,
          connectedDevice?.mtuNow ?? 23,
          (p) => update(
              p == 1
                  ? 'assistance accepted — waiting for GPS fix'
                  : 'sending to GPS...',
              p),
          cancelled: cancelled);
    } finally {
      _routeBusy = false;
    }
  }

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
  Future<String> _routeCommand(List<int> bytes) async {
    if (_commandBusy) throw StateError('Device command busy. Try again.');
    _commandBusy = true;
    try {
      final c = _routeControlChar;
      if (c == null) {
        throw StateError('Connect to a device with navigation firmware');
      }
      await c.write(bytes, withoutResponse: false, allowLongWrite: true);
      return await waitForRouteReply(() async => utf8.decode(await c.read()));
    } finally {
      _commandBusy = false;
    }
  }

  Future<List<SavedRide>> savedRides() async {
    if (_routeBusy) throw StateError('Wait for transfer');
    _routeBusy = true;
    try {
      return await listSavedRides(_routeCommand);
    } finally {
      _routeBusy = false;
    }
  }

  Future<Uint8List> exportRide(SavedRide ride, void Function(double) progress,
      {bool Function()? cancelled}) async {
    if (_routeBusy) throw StateError('Wait for transfer');
    _routeBusy = true;
    try {
      final export = _rideExportChar;
      if (export != null) {
        await export.setNotifyValue(true);
        try {
          return await downloadRideFast(ride, _routeCommand,
              export.onValueReceived, connectedDevice?.mtuNow ?? 23, progress,
              cancelled: cancelled,
              diagnostic: (message) => debugPrint('[GPX] $message'));
        } finally {
          try {
            await export.setNotifyValue(false);
          } catch (_) {}
        }
      }
      return await downloadRide(ride, _routeCommand, progress,
          cancelled: cancelled);
    } finally {
      _routeBusy = false;
    }
  }

  Future<String> sensorDebug({bool scan = false}) async {
    if (_routeBusy) throw StateError('Wait for transfer');
    _routeBusy = true;
    try {
      if (scan) return await _routeCommand([0x22]);
      return '${await _routeCommand([
            0x20
          ])}\nCSC sensors\n${await _routeCommand([0x21])}';
    } finally {
      _routeBusy = false;
    }
  }

  Future<void> freeRide() async {
    if (_routeBusy) throw StateError('Wait for route transfer');
    await _routeCommand([5]);
  }

  Future<void> selectRoute(RouteModel route) async {
    if (_routeBusy) throw StateError('Wait for route transfer');
    final b = ByteData(5)
      ..setUint8(0, 4)
      ..setUint32(1, RouteModel.checksum(route.toBytes()), Endian.little);
    await _routeCommand(b.buffer.asUint8List());
  }

  Future<String> routeMapCoverage(RouteModel route) async {
    if (_routeBusy) throw StateError('Wait for route transfer');
    final b = ByteData(5)
      ..setUint8(0, 6)
      ..setUint32(1, RouteModel.checksum(route.toBytes()), Endian.little);
    return _routeCommand(b.buffer.asUint8List());
  }

  Future<void> syncRoute(RouteModel route, void Function(double) progress,
      {bool Function()? cancelled}) async {
    if (_routeBusy) throw StateError('A route transfer is already running');
    final c = _routeControlChar, d = _routeDataChar, device = connectedDevice;
    if (c == null || d == null || device == null) {
      throw StateError('Connect to a device with navigation firmware');
    }
    _routeBusy = true;
    try {
      await transferRoute(
          bytes: route.toBytes(),
          mtu: device.mtuNow,
          command: _routeCommand,
          data: (packet) async {
            await d.write(packet, withoutResponse: false);
            return waitForRouteReply(() async => utf8.decode(await c.read()));
          },
          progress: progress,
          cancelled: cancelled);
    } finally {
      _routeBusy = false;
    }
  }

  Future<void> sendCommand(int cmd) async {
    if (_commandChar == null) return;
    try {
      await _commandChar!.write([cmd], withoutResponse: false);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to send command: $e");
    }
  }

  @override
  Future<void> writePhoneGpsSample(
      double lat, double lon, double accuracyM, int seq) async {
    final c = _phoneGpsChar;
    if (c == null) return;
    try {
      await c.write(encodePhoneGpsSample(lat, lon, accuracyM, seq),
          withoutResponse: true);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write phone GPS sample: $e");
    }
  }

  @override
  Future<int?> readGpsSourceMode() async {
    final c = _gpsSourceModeChar;
    if (c == null) return null;
    try {
      final v = await c.read();
      return v.isNotEmpty ? v[0] : null;
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to read GPS source mode: $e");
      return null;
    }
  }

  @override
  Future<void> writeGpsSourceMode(int mode) async {
    final c = _gpsSourceModeChar;
    if (c == null) return;
    try {
      await c.write([mode], withoutResponse: false);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write GPS source mode: $e");
    }
  }

  // --- WIRELESS OTA FIRMWARE FLASHER ---
  Stream<double> flashFirmware(Uint8List firmwareBytes) async* {
    if (_routeBusy) {
      throw StateError('Wait for the route transfer before updating firmware');
    }
    if (_otaControlChar == null || _otaDataChar == null) {
      throw Exception(
          "OTA GATT characteristics not found on connected device.");
    }

    final int totalBytes = firmwareBytes.length;
    yield 0.0;

    // Step 1: Send Begin Command (0x01 + 4-byte size)
    final beginPayload = ByteData(5);
    beginPayload.setUint8(0, BleProtocol.otaCmdBegin);
    beginPayload.setUint32(1, totalBytes, Endian.little);

    await _otaControlChar!
        .write(beginPayload.buffer.asUint8List(), withoutResponse: false);
    await Future.delayed(const Duration(milliseconds: 200));

    // Step 2: Stream binary chunks (480 bytes per packet)
    const int chunkSize = 480;
    int offset = 0;

    while (offset < totalBytes) {
      final int end =
          (offset + chunkSize < totalBytes) ? offset + chunkSize : totalBytes;
      final chunk = firmwareBytes.sublist(offset, end);

      await _otaDataChar!.write(chunk, withoutResponse: true);
      offset = end;

      final progress = offset / totalBytes;
      yield progress;

      // Small throttle to avoid buffer congestion
      await Future.delayed(const Duration(milliseconds: 8));
    }

    // Step 3: Send End Command (0x02) to verify & reboot
    await _otaControlChar!
        .write([BleProtocol.otaCmdEnd], withoutResponse: false);
    yield 1.0;
  }
}
