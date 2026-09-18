import 'dart:io';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

enum BleAccess {
  unknown,
  ready,
  denied,
  blocked,
  off,
  locationOff,
  unsupported
}

/// Permission checks are separate from scanning: opening the app never scans
/// or connects without an explicit user action.
class BleAccessGate {
  static const channel = MethodChannel('opencyclo/ble_access');
  final bool _isAndroid;
  final Future<void> Function() _turnOn;
  Future<BleAccess>? _pending;

  BleAccessGate({bool? isAndroid, Future<void> Function()? turnOn})
      : _isAndroid = isAndroid ?? Platform.isAndroid,
        _turnOn = turnOn ?? FlutterBluePlus.turnOn;

  Future<BleAccess> check({bool request = false}) {
    return _pending ??= _check(request).whenComplete(() => _pending = null);
  }

  Future<BleAccess> _check(bool request) async {
    // Preserve existing behavior on other platforms; this gate is Android-only.
    if (!_isAndroid) return BleAccess.ready;
    var value =
        await channel.invokeMethod<String>(request ? 'request' : 'check');
    if (request && value == 'off') {
      try {
        await _turnOn();
      } catch (_) {
        // User can dismiss the system dialog. Recheck, never start scanning.
      }
      value = await channel.invokeMethod<String>('check');
    }
    return BleAccess.values.firstWhere((state) => state.name == value,
        orElse: () => BleAccess.unknown);
  }

  Future<void> openSettings() async {
    if (_isAndroid) await channel.invokeMethod<void>('settings');
  }
}
