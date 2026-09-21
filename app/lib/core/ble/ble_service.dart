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
import 'screenshot_download.dart';
import 'gps_assistance.dart';
import 'gps_registration.dart';
import 'gps_credentials.dart';
import 'gps_cache.dart';
import '../../state/gps_source_provider.dart' show GpsSourceBleChannel;
import '../../state/baro_source_provider.dart' show BaroSourceBleChannel;
import '../../state/compass_source_provider.dart' show CompassSourceBleChannel;

class BleService
    implements GpsSourceBleChannel, BaroSourceBleChannel, CompassSourceBleChannel {
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
  BluetoothCharacteristic? _phoneBaroChar;
  BluetoothCharacteristic? _baroSourceModeChar;
  BluetoothCharacteristic? _phoneCompassChar;
  BluetoothCharacteristic? _compassSourceModeChar;
  bool _routeBusy = false;
  bool _commandBusy = false;

  final _telemetryController = StreamController<TelemetryModel>.broadcast();
  Stream<TelemetryModel> get telemetryStream => _telemetryController.stream;

  final _gpsSourceModeController = StreamController<int>.broadcast();
  @override
  Stream<int> get gpsSourceModeStream => _gpsSourceModeController.stream;

  final _baroSourceModeController = StreamController<int>.broadcast();
  @override
  Stream<int> get baroSourceModeStream => _baroSourceModeController.stream;

  final _compassSourceModeController = StreamController<int>.broadcast();
  @override
  Stream<int> get compassSourceModeStream => _compassSourceModeController.stream;

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
      _phoneBaroChar = null;
      _baroSourceModeChar = null;
      _phoneCompassChar = null;
      _compassSourceModeChar = null;
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
          _phoneBaroChar = null;
          _baroSourceModeChar = null;
          _phoneCompassChar = null;
          _compassSourceModeChar = null;
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

      // Discover Services. Match the full UUIDs from BleProtocol rather than
      // digit substrings: a substring match silently binds to any unrelated
      // service/characteristic whose UUID text happens to contain "1901".."190f".
      final services = await device.discoverServices();
      for (final service in services) {
        final sUuid = service.uuid.toString().toLowerCase();
        if (sUuid == BleProtocol.openCycloServiceUuid) {
          for (final char in service.characteristics) {
            switch (char.uuid.toString().toLowerCase()) {
              case BleProtocol.layoutConfigCharUuid:
                _layoutChar = char;
                // The device answers every layout write with OK/ERR on this
                // characteristic's notify leg; without the subscription a
                // rejected layout still looks like a successful sync.
                await char.setNotifyValue(true);
                break;
              case BleProtocol.telemetryStreamCharUuid:
                await _subscribeTelemetry(char);
                break;
              case BleProtocol.deviceCommandCharUuid:
                _commandChar = char;
                break;
              case BleProtocol.routeControlCharUuid:
                _routeControlChar = char;
                break;
              case BleProtocol.routeDataCharUuid:
                _routeDataChar = char;
                break;
              case BleProtocol.exportCharUuid:
                _rideExportChar = char;
                break;
              case BleProtocol.gpsAssistanceCharUuid:
                _gpsAssistanceChar = char;
                break;
              case BleProtocol.gpsIdentityCharUuid:
                _gpsIdentityChar = char;
                break;
              case BleProtocol.gpsCacheCharUuid:
                _gpsCacheChar = char;
                break;
              case BleProtocol.phoneGpsCharUuid:
                _phoneGpsChar = char;
                break;
              case BleProtocol.gpsSourceModeCharUuid:
                _gpsSourceModeChar = char;
                await _subscribeGpsSourceMode(char);
                break;
              case BleProtocol.phoneBaroCharUuid:
                _phoneBaroChar = char;
                break;
              case BleProtocol.phoneCompassCharUuid:
                _phoneCompassChar = char;
                break;
              case BleProtocol.baroSourceModeCharUuid:
                _baroSourceModeChar = char;
                await _subscribeModeChar(char, _baroSourceModeController);
                break;
              case BleProtocol.compassSourceModeCharUuid:
                _compassSourceModeChar = char;
                await _subscribeModeChar(char, _compassSourceModeController);
                break;
            }
          }
        } else if (sUuid == BleProtocol.otaServiceUuid) {
          for (final char in service.characteristics) {
            switch (char.uuid.toString().toLowerCase()) {
              case BleProtocol.otaControlCharUuid:
                _otaControlChar = char;
                // Every OTA control write is answered with a {status, code}
                // notification; without this subscription the flasher cannot
                // tell a rejected update from a successful one.
                await char.setNotifyValue(true);
                break;
              case BleProtocol.otaDataCharUuid:
                _otaDataChar = char;
                break;
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
      _phoneBaroChar = null;
      _baroSourceModeChar = null;
      _phoneCompassChar = null;
      _compassSourceModeChar = null;
    }
  }

  Future<void> _subscribeTelemetry(BluetoothCharacteristic char) async {
    await char.setNotifyValue(true);
    char.lastValueStream.listen((value) {
      if (value.isEmpty) return;
      try {
        _telemetryController.add(TelemetryModel.fromBytes(value));
      } on FormatException catch (e) {
        debugPrint('[BLE ERROR] Dropping malformed telemetry packet: $e');
      }
    });
  }

  Future<void> _subscribeGpsSourceMode(BluetoothCharacteristic char) async {
    await char.setNotifyValue(true);
    char.lastValueStream.listen((value) {
      if (value.isNotEmpty) _gpsSourceModeController.add(value[0]);
    });
  }

  Future<void> _subscribeModeChar(
      BluetoothCharacteristic char, StreamController<int> controller) async {
    await char.setNotifyValue(true);
    char.lastValueStream.listen((value) {
      if (value.isNotEmpty) controller.add(value[0]);
    });
  }

  // --- LAYOUT CONFIGURATION SYNC ---

  /// Writes [payload] and waits for the single reply the firmware sends on the
  /// same characteristic's notify leg (0x1901 layout OK/ERR, 0x1911 OTA
  /// result). Both callers are one-shot and serialised, so one outstanding
  /// request at a time is enough.
  Future<List<int>> _writeForAck(
    BluetoothCharacteristic char,
    List<int> payload, {
    required String what,
    bool allowLongWrite = false,
    Duration timeout = const Duration(seconds: 5),
  }) async {
    final reply = char.lastValueStream.first.timeout(
          timeout,
          onTimeout: () => throw StateError('$what: no reply from the device'),
        );
    await char.write(payload,
        withoutResponse: false, allowLongWrite: allowLongWrite);
    return reply;
  }

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
    final c = _layoutChar;
    if (c == null) return false;
    try {
      final bytes = utf8.encode(config.toJsonString());
      // allowLongWrite, matching _routeCommand: without it the platform layer
      // rejects anything longer than MTU-3 before it reaches the device, which
      // Android's 512-byte MTU hides and iOS/macOS do not.
      final reply = await _writeForAck(c, bytes,
          what: 'Layout sync', allowLongWrite: true);
      final text = utf8.decode(reply, allowMalformed: true);
      if (text != 'OK') {
        debugPrint('[BLE ERROR] Device rejected layout: $text');
        return false;
      }
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

  /// Windowed binary export over the 0x1906 notifications. `openCommand`
  /// picks the firmware folder: 0x12 for rides, 0x17 for screenshots.
  Future<Uint8List> _fastExport(BluetoothCharacteristic export, SavedRide file,
      void Function(double) progress,
      {required int openCommand,
      required String tag,
      bool Function()? cancelled}) async {
    await export.setNotifyValue(true);
    try {
      return await downloadRideFast(file, _routeCommand, export.onValueReceived,
          connectedDevice?.mtuNow ?? 23, progress,
          cancelled: cancelled,
          openCommand: openCommand,
          diagnostic: (message) => debugPrint('[$tag] $message'));
    } finally {
      try {
        await export.setNotifyValue(false);
      } catch (_) {}
    }
  }

  Future<Uint8List> exportRide(SavedRide ride, void Function(double) progress,
      {bool Function()? cancelled}) async {
    if (_routeBusy) throw StateError('Wait for transfer');
    _routeBusy = true;
    try {
      final export = _rideExportChar;
      if (export != null) {
        return await _fastExport(export, ride, progress,
            openCommand: 0x12, tag: 'GPX', cancelled: cancelled);
      }
      return await downloadRide(ride, _routeCommand, progress,
          cancelled: cancelled);
    } finally {
      _routeBusy = false;
    }
  }

  /// The device's most recent screenshot as raw BMP bytes, with its name, or
  /// null when it has none this boot. Same binary export session as rides.
  Future<(SavedRide, Uint8List)?> exportLastScreenshot(
      void Function(double) progress,
      {bool Function()? cancelled}) async {
    if (_routeBusy) throw StateError('Wait for transfer');
    _routeBusy = true;
    try {
      final shot = await lastScreenshot(_routeCommand);
      if (shot == null) return null;
      final export = _rideExportChar;
      if (export == null) {
        throw StateError('Connect to a device with binary export firmware');
      }
      final bytes = await _fastExport(export, shot, progress,
          openCommand: 0x17, tag: 'SHOT', cancelled: cancelled);
      return (shot, bytes);
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
  Future<void> writePhoneGpsSample(double lat, double lon, double accuracyM,
      int seq, int utcEpochS) async {
    final c = _phoneGpsChar;
    if (c == null) return;
    try {
      await c.write(
          encodePhoneGpsSample(lat, lon, accuracyM, seq, utcEpochS),
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

  @override
  Future<void> writePhoneAltitudeSample(double altitudeM, int seq) async {
    final c = _phoneBaroChar;
    if (c == null) return;
    try {
      await c.write(encodePhoneAltitudeSample(altitudeM, seq),
          withoutResponse: true);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write phone altitude sample: $e");
    }
  }

  @override
  Future<int?> readBaroSourceMode() async {
    final c = _baroSourceModeChar;
    if (c == null) return null;
    try {
      final v = await c.read();
      return v.isNotEmpty ? v[0] : null;
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to read baro source mode: $e");
      return null;
    }
  }

  @override
  Future<void> writeBaroSourceMode(int mode) async {
    final c = _baroSourceModeChar;
    if (c == null) return;
    try {
      await c.write([mode], withoutResponse: false);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write baro source mode: $e");
    }
  }

  @override
  Future<void> writePhoneHeadingSample(
      double headingDeg, int accuracy, int seq) async {
    final c = _phoneCompassChar;
    if (c == null) return;
    try {
      await c.write(encodePhoneHeadingSample(headingDeg, accuracy, seq),
          withoutResponse: true);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write phone heading sample: $e");
    }
  }

  @override
  Future<int?> readCompassSourceMode() async {
    final c = _compassSourceModeChar;
    if (c == null) return null;
    try {
      final v = await c.read();
      return v.isNotEmpty ? v[0] : null;
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to read compass source mode: $e");
      return null;
    }
  }

  @override
  Future<void> writeCompassSourceMode(int mode) async {
    final c = _compassSourceModeChar;
    if (c == null) return;
    try {
      await c.write([mode], withoutResponse: false);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write compass source mode: $e");
    }
  }

  // --- WIRELESS OTA FIRMWARE FLASHER ---
  Stream<double> flashFirmware(Uint8List firmwareBytes) async* {
    if (_routeBusy) {
      throw StateError('Wait for the route transfer before updating firmware');
    }
    final control = _otaControlChar;
    final data = _otaDataChar;
    if (control == null || data == null) {
      throw Exception(
          "OTA GATT characteristics not found on connected device.");
    }

    final int totalBytes = firmwareBytes.length;
    yield 0.0;

    try {
      // Step 1: Send Begin Command (0x01 + 4-byte size) and wait for the
      // device's ready/failed reply on 0x1911 before streaming anything.
      final beginPayload = ByteData(5);
      beginPayload.setUint8(0, BleProtocol.otaCmdBegin);
      beginPayload.setUint32(1, totalBytes, Endian.little);

      final begin = await _writeForAck(
          control, beginPayload.buffer.asUint8List(),
          what: 'Firmware update begin');
      if (begin.isEmpty || begin[0] != BleProtocol.otaStatusReady) {
        throw Exception('Device refused the update: ${describeOtaFailure(begin)}');
      }

      // Step 2: Stream binary chunks, sized from the negotiated MTU rather than
      // a constant: 480 bytes only fits the Android 512-byte MTU, and on
      // iOS/macOS the platform rejects larger-than-MTU payloads before they
      // ever reach the radio.
      final int chunkSize = ((connectedDevice?.mtuNow ?? 23) - 3).clamp(1, 480);
      int offset = 0;

      while (offset < totalBytes) {
        final int end =
            (offset + chunkSize < totalBytes) ? offset + chunkSize : totalBytes;
        final chunk = firmwareBytes.sublist(offset, end);

        await data.write(chunk, withoutResponse: true);
        offset = end;

        final progress = offset / totalBytes;
        yield progress;

        // Small throttle to avoid buffer congestion
        await Future.delayed(const Duration(milliseconds: 8));
      }

      // Step 3: Send End Command (0x02) to verify & reboot. The device only
      // commits after this reply; a byte-count mismatch or a failed
      // Update.end arrives here as 0xFF.
      final end = await _writeForAck(
          control, [BleProtocol.otaCmdEnd],
          what: 'Firmware update finalize',
          timeout: const Duration(seconds: 30));
      if (end.isEmpty || end[0] != BleProtocol.otaStatusDone) {
        throw Exception('Device rejected the firmware image: ${describeOtaFailure(end)}');
      }
      yield 1.0;
    } catch (e) {
      // Best effort: an aborted stream must not leave the device holding the
      // update slot until it disconnects.
      try {
        await control.write([BleProtocol.otaCmdAbort], withoutResponse: false);
      } catch (_) {
        // The link may already be gone; the original error is what matters.
      }
      rethrow;
    }
  }
}
