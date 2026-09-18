import 'package:flutter/material.dart';
import 'dart:async';
import '../../core/ble/ble_service.dart';
import '../theme/app_theme.dart';
import '../widgets/device_ui.dart';

class GpsAssistanceScreen extends StatefulWidget {
  const GpsAssistanceScreen({super.key});
  @override
  State<GpsAssistanceScreen> createState() => _GpsAssistanceScreenState();
}

class _GpsAssistanceScreenState extends State<GpsAssistanceScreen> {
  final _chipcode = TextEditingController();
  final _ztpToken = TextEditingController();
  String? _registeredDevice;
  bool _busy = false, _cancelled = false;
  double _progress = 0;
  String _status = 'not synced';
  String _cacheStatus = 'checking cache...';
  Timer? _timer;
  @override
  void initState() {
    super.initState();
    if (BleService.instance.connectedDevice != null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) _credentials();
      });
      _timer = Timer.periodic(const Duration(seconds: 2), (_) {
        if (!_busy) _refreshCache();
      });
    }
  }

  Future<void> _refreshCache() async {
    try {
      final s = await BleService.instance.gpsCacheStatus();
      if (mounted) {
        setState(() => _cacheStatus =
            '${s.label}${s.expiry.isEmpty ? '' : ' · until ${s.expiry} UTC'}');
      }
    } catch (_) {
      if (mounted) {
        setState(
            () => _cacheStatus = 'cache unavailable — connect/update firmware');
      }
    }
  }

  Future<void> _credentials({bool save = false, bool forget = false}) async {
    setState(() => _busy = true);
    try {
      final code = await BleService.instance
          .savedGpsChipcode(save: save ? _chipcode.text : null, forget: forget);
      if (mounted) {
        setState(() {
          _chipcode.text = code ?? '';
          _registeredDevice = BleService.instance.connectedDevice?.remoteId.str;
          _status = forget
              ? 'saved Chipcode removed'
              : code == null
                  ? 'register GPS or enter Chipcode'
                  : 'Chipcode loaded securely';
        });
      }
    } catch (_) {
      if (mounted) {
        setState(() => _status =
            'Could not access saved Chipcode. Check GPS connection and secure storage.');
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
    await _refreshCache();
  }

  @override
  void dispose() {
    _cancelled = true;
    _timer?.cancel();
    _chipcode.dispose();
    _ztpToken.dispose();
    super.dispose();
  }

  Future<void> _sync({bool cache = false}) async {
    if (_registeredDevice != null &&
        _registeredDevice !=
            BleService.instance.connectedDevice?.remoteId.str) {
      setState(() => _status =
          'Device changed. Register this receiver or enter its Chipcode.');
      return;
    }
    setState(() {
      _busy = true;
      _cancelled = false;
      _progress = 0;
    });
    try {
      final operation = cache
          ? BleService.instance.syncGpsCache
          : BleService.instance.syncGps;
      await operation(_chipcode.text, (status, progress) {
        if (mounted) {
          setState(() {
            _status = status;
            _progress = progress;
          });
        }
      }, cancelled: () => _cancelled);
    } catch (error) {
      if (mounted) setState(() => _status = error.toString());
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) => Scaffold(
        appBar: AppBar(title: const Text('assisted GPS')),
        body: ListView(padding: const EdgeInsets.all(16), children: [
          const Text(
              'Sync before riding. Your phone needs internet and a BLE connection. '
              'After sync, GPS works without the app staying open.'),
          const SizedBox(height: 16),
          ExpansionTile(
            title: const Text('register GPS'),
            children: [
              const Text(
                  'Use a new ZTP token from your Evaluation profile. Register sends '
                  'the token and receiver identity to Thingstream and creates a device entry. '
                  'Chipcode is saved securely per receiver; the ZTP token is not saved. Cancellation cannot undo a registration already submitted.'),
              TextField(
                  controller: _ztpToken,
                  enabled: !_busy,
                  obscureText: true,
                  autocorrect: false,
                  enableSuggestions: false,
                  decoration: const InputDecoration(labelText: 'ZTP token')),
              const SizedBox(height: 12),
              DeviceButton(
                  text: 'register GPS',
                  icon: Icons.app_registration,
                  color: AppTheme.cyan,
                  onPressed: _busy ? null : _register),
              const SizedBox(height: 16),
            ],
          ),
          TextField(
              controller: _chipcode,
              onChanged: (_) => _registeredDevice = null,
              enabled: !_busy,
              obscureText: true,
              autocorrect: false,
              enableSuggestions: false,
              decoration: const InputDecoration(
                  labelText: 'AssistNow Chipcode',
                  helperMaxLines: 3,
                  helperText:
                      'Saved securely per GPS after registration, or tap save below.')),
          Wrap(spacing: 8, children: [
            TextButton(
                onPressed: _busy ? null : () => _credentials(save: true),
                child: const Text('save')),
            TextButton(
                onPressed: _busy ? null : () => _credentials(),
                child: const Text('load')),
            TextButton(
                onPressed: _busy ? null : () => _credentials(forget: true),
                child: const Text('forget')),
          ]),
          const SizedBox(height: 16),
          DeviceButton(
              text: 'sync GPS',
              icon: Icons.satellite_alt,
              color: AppTheme.cyan,
              busy: _busy,
              onPressed: _busy ? null : () => _sync()),
          const SizedBox(height: 8),
          DeviceButton(
              text: 'cache 7 days',
              icon: Icons.offline_bolt_outlined,
              color: AppTheme.cyan,
              onPressed: _busy ? null : () => _sync(cache: true)),
          const SizedBox(height: 8),
          Text(_cacheStatus, style: AppTheme.labelStyle),
          if (_busy) ...[
            const SizedBox(height: 12),
            LinearProgressIndicator(value: _progress == 0 ? null : _progress),
            TextButton(
                onPressed: () => setState(() {
                      _cancelled = true;
                      _status = 'cancelling...';
                    }),
                child: const Text('cancel')),
          ],
          const SizedBox(height: 16),
          Text(_status, style: AppTheme.labelStyle),
          const SizedBox(height: 16),
          const Text(
              'Requires an AssistNow-registered receiver with Live Orbits access. '
              'Predictive cache needs Predictive Orbits access and a working SD card. '
              'Cache is used automatically when its date and the device clock are valid. '
              'Accepted assistance is not a position fix; keep the GPS under open sky.'),
        ]),
      );

  Future<void> _register() async {
    final device = BleService.instance.connectedDevice?.remoteId.str;
    setState(() {
      _busy = true;
      _cancelled = false;
      _progress = 0;
    });
    try {
      final code =
          await BleService.instance.registerGps(_ztpToken.text, (status) {
        if (mounted) setState(() => _status = status);
      }, cancelled: () => _cancelled);
      if (mounted) {
        setState(() {
          _chipcode.text = code;
          _registeredDevice = device;
          _status = 'registered — tap sync GPS';
        });
      }
    } catch (error) {
      if (mounted) setState(() => _status = error.toString());
    } finally {
      if (mounted) {
        _ztpToken.clear();
        setState(() => _busy = false);
      }
    }
  }
}
