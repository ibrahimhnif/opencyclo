import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import '../../core/ble/ble_service.dart';

class SensorDebugScreen extends StatefulWidget {
  const SensorDebugScreen({super.key});
  @override
  State<SensorDebugScreen> createState() => _SensorDebugScreenState();
}

class _SensorDebugScreenState extends State<SensorDebugScreen>
    with WidgetsBindingObserver {
  Timer? timer;
  bool busy = false, live = false;
  String snapshot = 'tap refresh to inspect sensors';
  final List<String> history = [];

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state != AppLifecycleState.resumed) {
      timer?.cancel();
      if (mounted) setState(() => live = false);
    }
  }

  @override
  void dispose() {
    timer?.cancel();
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  Future<void> refresh({bool scan = false}) async {
    if (busy) return;
    setState(() => busy = true);
    try {
      final text = await BleService.instance.sensorDebug(scan: scan);
      if (!mounted) return;
      setState(() {
        snapshot = text;
        history.add('${DateTime.now().toUtc().toIso8601String()}\n$text');
        if (history.length > 300) history.removeAt(0);
      });
    } catch (error) {
      if (mounted) {
        setState(() {
          snapshot = '$error';
          live = false;
          timer?.cancel();
        });
      }
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  @override
  Widget build(BuildContext context) => Scaffold(
        appBar: AppBar(title: const Text('sensor debug')),
        body: ListView(padding: const EdgeInsets.all(16), children: [
          const Text(
              'source: speed 0 none / 1 GPS / 2 wheel; altitude 0 none / 1 baro / 2 GPS. Barometer uses standard pressure, not calibrated elevation.'),
          SwitchListTile(
              title: const Text('live · 2 seconds'),
              value: live,
              onChanged: (value) {
                timer?.cancel();
                setState(() => live = value);
                if (value) {
                  refresh();
                  timer = Timer.periodic(
                      const Duration(seconds: 2), (_) => refresh());
                }
              }),
          Wrap(spacing: 8, children: [
            OutlinedButton(
                onPressed: busy ? null : () => refresh(),
                child: const Text('refresh')),
            OutlinedButton(
                onPressed: busy ? null : () => refresh(scan: true),
                child: const Text('scan sensors')),
            OutlinedButton(
                onPressed: history.isEmpty
                    ? null
                    : () async {
                        try {
                          await FilePicker.platform.saveFile(
                              fileName: 'opencyclo-debug.txt',
                              type: FileType.any,
                              bytes: Uint8List.fromList(
                                  utf8.encode(history.join('\n\n'))));
                        } catch (error) {
                          if (mounted) setState(() => snapshot = '$error');
                        }
                      },
                child: const Text('save debug')),
          ]),
          const SizedBox(height: 16),
          SelectableText(snapshot,
              style: const TextStyle(fontFamily: 'monospace', fontSize: 13)),
          const SizedBox(height: 16),
          const Text(
              'Spin both wheel and crank to wake sensors. CSC slots identify capabilities from notifications; paired is not the same as streaming. Live debug stops when leaving this screen or backgrounding the app.'),
        ]),
      );
}
