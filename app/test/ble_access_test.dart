import 'dart:async';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/ble/ble_access.dart';
import 'package:opencyclo_app/state/ble_provider.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
  tearDown(
      () => messenger.setMockMethodCallHandler(BleAccessGate.channel, null));

  for (final access in BleAccess.values) {
    test('check preserves ${access.name} without requesting or enabling',
        () async {
      messenger.setMockMethodCallHandler(BleAccessGate.channel, (call) async {
        expect(call.method, 'check');
        return access.name;
      });
      final gate = BleAccessGate(
          isAndroid: true, turnOn: () async => fail('unexpected enable'));
      expect(await gate.check(), access);
    });
  }

  test('request denied or blocked never enables Bluetooth', () async {
    for (final status in ['denied', 'blocked']) {
      messenger.setMockMethodCallHandler(BleAccessGate.channel, (call) async {
        expect(call.method, 'request');
        return status;
      });
      final gate = BleAccessGate(
          isAndroid: true, turnOn: () async => fail('unexpected enable'));
      expect((await gate.check(request: true)).name, status);
    }
  });

  test('off requests enable then rechecks; cancellation stays off', () async {
    for (final accepted in [true, false]) {
      var enabled = false;
      final calls = <String>[];
      messenger.setMockMethodCallHandler(BleAccessGate.channel, (call) async {
        calls.add(call.method);
        return enabled ? 'ready' : 'off';
      });
      final gate = BleAccessGate(
          isAndroid: true,
          turnOn: () async {
            if (!accepted) throw PlatformException(code: 'cancelled');
            enabled = true;
          });
      expect(await gate.check(request: true),
          accepted ? BleAccess.ready : BleAccess.off);
      expect(calls, ['request', 'check']);
    }
  });

  test('concurrent checks share one permission request', () async {
    final reply = Completer<String>();
    var calls = 0;
    messenger.setMockMethodCallHandler(BleAccessGate.channel, (call) {
      calls++;
      return reply.future;
    });
    final gate = BleAccessGate(isAndroid: true);
    final first = gate.check(request: true);
    final second = gate.check(request: true);
    reply.complete('ready');
    expect(await first, BleAccess.ready);
    expect(await second, BleAccess.ready);
    expect(calls, 1);
  });

  test('settings is an explicit action', () async {
    messenger.setMockMethodCallHandler(BleAccessGate.channel, (call) async {
      expect(call.method, 'settings');
      return null;
    });
    await BleAccessGate(isAndroid: true).openSettings();
  });

  test('unknown native status fails closed', () async {
    messenger.setMockMethodCallHandler(
        BleAccessGate.channel, (_) async => 'invalid');
    expect(await BleAccessGate(isAndroid: true).check(), BleAccess.unknown);
  });

  test('disconnect can clear selected device', () {
    final state =
        BleState(connectedDevice: BluetoothDevice.fromId('AA:BB:CC:DD:EE:FF'));
    expect(state.copyWith().connectedDevice, isNotNull);
    expect(state.copyWith(clearDevice: true).connectedDevice, isNull);
  });

  test('denied access prevents scan and connection from starting', () async {
    messenger.setMockMethodCallHandler(
        BleAccessGate.channel, (_) async => 'denied');
    final notifier = BleNotifier(accessGate: BleAccessGate(isAndroid: true));
    addTearDown(notifier.dispose);
    await notifier.startScan();
    expect(notifier.state.access, BleAccess.denied);
    expect(notifier.state.isScanning, isFalse);
    expect(notifier.state.errorMessage, isEmpty);
    await notifier.connect(BluetoothDevice.fromId('AA:BB:CC:DD:EE:FF'));
    expect(notifier.state.status, DeviceConnectionStatus.disconnected);
    expect(notifier.state.connectedDevice, isNull);
    expect(notifier.state.errorMessage, isEmpty);
  });

  test('native errors fail closed and allow retry', () async {
    messenger.setMockMethodCallHandler(BleAccessGate.channel, (_) async {
      throw PlatformException(code: 'unavailable');
    });
    final notifier = BleNotifier(accessGate: BleAccessGate(isAndroid: true));
    addTearDown(notifier.dispose);
    expect(await notifier.checkAccess(request: true), isFalse);
    expect(notifier.state.access, BleAccess.unknown);
    expect(notifier.state.checkingAccess, isFalse);
    messenger.setMockMethodCallHandler(
        BleAccessGate.channel, (_) async => 'ready');
    expect(await notifier.checkAccess(request: true), isTrue);
    expect(notifier.state.errorMessage, isEmpty);
  });
}
