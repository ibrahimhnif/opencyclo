import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../state/ble_provider.dart';
import '../../../state/gps_source_provider.dart';
import '../../theme/app_theme.dart';
import '../../widgets/device_ui.dart';
import '../sensor_debug_screen.dart';
import '../gps_assistance_screen.dart';

/// Mirrors `renderWidgetBleManager` on the device.
///
/// Same structure, top to bottom: a full-width scan button that turns amber
/// while scanning, then a list of sensor rows carrying a mac address and a
/// destructive chip, then a muted status footer. The device says "scanning..."
/// and "not paired"; so does this.
class DeviceTab extends ConsumerStatefulWidget {
  const DeviceTab({super.key});

  @override
  ConsumerState<DeviceTab> createState() => _DeviceTabState();
}

class _DeviceTabState extends ConsumerState<DeviceTab>
    with WidgetsBindingObserver {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) ref.read(bleProvider.notifier).checkAccess(request: true);
    });
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      // Refresh after Settings without reopening permission/enable dialogs.
      ref.read(bleProvider.notifier).checkAccess();
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final bleState = ref.watch(bleProvider);
    final bleNotifier = ref.read(bleProvider.notifier);

    final gpsSourceState = ref.watch(gpsSourceProvider);
    final gpsSourceNotifier = ref.read(gpsSourceProvider.notifier);

    ref.listen<BleState>(bleProvider, (previous, next) {
      final justConnected = next.status == DeviceConnectionStatus.connected &&
          previous?.status != DeviceConnectionStatus.connected;
      if (justConnected) gpsSourceNotifier.loadFromDevice();
    });

    final connected = bleState.status == DeviceConnectionStatus.connected;
    final connecting = bleState.status == DeviceConnectionStatus.connecting;

    final String linkText;
    final Color linkColor;
    if (connected) {
      linkText = 'linked';
      linkColor = AppTheme.green;
    } else if (connecting) {
      linkText = 'linking...';
      linkColor = AppTheme.amber;
    } else {
      linkText = 'no link';
      linkColor = AppTheme.label;
    }

    return DeviceScreen(
      statusBar: DeviceStatusBar(
        title: 'device',
        linkText: linkText,
        linkColor: linkColor,
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(
          AppTheme.gutter,
          8,
          AppTheme.gutter,
          24,
        ),
        children: [
          if (bleState.access != BleAccess.ready) ...[
            Text(
                switch (bleState.access) {
                  BleAccess.denied =>
                    'allow bluetooth to find your opencyclo. older android versions require location permission.',
                  BleAccess.blocked =>
                    'bluetooth permission blocked. allow it in app settings.',
                  BleAccess.off => 'bluetooth is off. turn it on to connect.',
                  BleAccess.locationOff =>
                    'turn on location services for bluetooth scanning on this android version.',
                  BleAccess.unsupported =>
                    'bluetooth le is not available on this device.',
                  _ => 'checking bluetooth access...',
                },
                style: AppTheme.labelStyle),
            const SizedBox(height: 8),
            DeviceButton(
              color: AppTheme.cyan,
              text: bleState.checkingAccess
                  ? 'checking...'
                  : switch (bleState.access) {
                      BleAccess.blocked || BleAccess.locationOff => 'settings',
                      BleAccess.off => 'enable bluetooth',
                      BleAccess.denied => 'allow bluetooth',
                      _ => 'retry',
                    },
              icon: Icons.bluetooth,
              busy: bleState.checkingAccess,
              onPressed: bleState.checkingAccess ||
                      bleState.access == BleAccess.unsupported
                  ? null
                  : () => bleNotifier.resolveAccess(),
            ),
            const SizedBox(height: 12),
          ],
          // The device draws its scan button first, cyan when idle and amber
          // while a scan is running. Same colour logic here.
          DeviceButton(
            text: bleState.isScanning ? 'scanning...' : 'scan',
            color: bleState.isScanning ? AppTheme.amber : AppTheme.cyan,
            icon: Icons.search,
            busy: bleState.isScanning,
            onPressed:
                bleState.checkingAccess || bleState.access != BleAccess.ready
                    ? null
                    : bleState.isScanning
                        ? () => bleNotifier.stopScan()
                        : () => bleNotifier.startScan(),
          ),
          const SizedBox(height: 8),

          // Paired device, shown as a settings-style row rather than a card —
          // the device has no cards.
          if (bleState.connectedDevice != null) ...[
            const DeviceSectionLabel(text: 'paired computer'),
            DeviceListRow(
              label: bleState.connectedDevice!.platformName.isNotEmpty
                  ? bleState.connectedDevice!.platformName
                  : 'opencyclo',
              detail: bleState.connectedDevice!.remoteId.str,
              trailing: DeviceChip(
                text: 'forget',
                color: AppTheme.red,
                onTap: () => bleNotifier.disconnect(),
              ),
              showDivider: false,
            ),
            const SizedBox(height: 8),
          ],

          DeviceSectionLabel(
            text: 'discovered',
            trailing: Text(
              '${bleState.scanResults.length}',
              style: AppTheme.statusStyle(
                bleState.scanResults.isEmpty ? AppTheme.label : AppTheme.cyan,
              ),
            ),
          ),

          if (bleState.scanResults.isEmpty)
            DeviceEmptyState(
              text: bleState.isScanning
                  ? 'searching for nearby opencyclo...'
                  : 'not paired. tap scan to search.',
            )
          else
            ...List.generate(bleState.scanResults.length, (index) {
              final result = bleState.scanResults[index];
              var name = result.device.platformName;
              if (name.isEmpty) name = result.advertisementData.advName;
              if (name.isEmpty) name = 'unknown device';

              final isOpenCyclo = name.toLowerCase().contains('opencyclo');
              final isLast = index == bleState.scanResults.length - 1;

              return DeviceListRow(
                label: name,
                detail:
                    '${result.device.remoteId.str}  ·  rssi ${result.rssi} dbm',
                showDivider: !isLast,
                trailing: DeviceChip(
                  // Green for the device we're looking for, muted cyan for
                  // anything else that happened to answer the scan.
                  text: 'connect',
                  color: isOpenCyclo ? AppTheme.green : AppTheme.cyan,
                  onTap: bleState.checkingAccess ||
                          connecting ||
                          bleState.access != BleAccess.ready
                      ? null
                      : () => bleNotifier.connect(result.device),
                ),
              );
            }),

          const SizedBox(height: 16),
          DeviceButton(
              text: 'sensor debug',
              icon: Icons.bug_report_outlined,
              color: AppTheme.cyan,
              onPressed: connected
                  ? () => Navigator.of(context).push(MaterialPageRoute<void>(
                      builder: (_) => const SensorDebugScreen()))
                  : null),
          const SizedBox(height: 8),
          DeviceButton(
              text: 'assisted GPS',
              icon: Icons.satellite_alt,
              color: AppTheme.cyan,
              onPressed: connected
                  ? () => Navigator.of(context).push(MaterialPageRoute<void>(
                      builder: (_) => const GpsAssistanceScreen()))
                  : null),
          const SizedBox(height: 16),

          DeviceSectionLabel(text: 'gps source'),
          DeviceListRow(
            label: gpsSourceState.mode == GpsSourceMode.hardware
                ? 'hardware (m10)'
                : 'phone',
            detail: gpsSourceState.mode == GpsSourceMode.hardware
                ? 'auto-falls back to phone if m10 has no fix'
                : 'always uses phone location; m10 ignored',
            trailing: DeviceChip(
              text: gpsSourceState.mode == GpsSourceMode.hardware
                  ? 'use phone'
                  : 'use hardware',
              color: AppTheme.cyan,
              onTap: connected && !gpsSourceState.syncing
                  ? () => gpsSourceNotifier.setMode(
                      gpsSourceState.mode == GpsSourceMode.hardware
                          ? GpsSourceMode.phoneForced
                          : GpsSourceMode.hardware)
                  : null,
            ),
            showDivider: false,
          ),
          // Phone position streams in both modes (hardware mode auto-falls
          // back to it), so a permission/service failure matters even when
          // 'hardware' is selected. Same muted voice as the link footer.
          if (gpsSourceState.error != null) ...[
            const SizedBox(height: 6),
            Text(
              DeviceText.normalise(gpsSourceState.error!),
              style: AppTheme.statusStyle(AppTheme.red),
            ),
          ],
          const SizedBox(height: 16),

          // The device closes its BLE page with a muted one-line status. So
          // does this — same voice, same colour.
          Text(
            connected
                ? 'link: gatt connected, telemetry streaming'
                : bleState.isScanning
                    ? 'link: searching...'
                    : 'link: not connected',
            style: AppTheme.labelStyle,
          ),
          if (bleState.errorMessage.isNotEmpty) ...[
            const SizedBox(height: 6),
            Text(
              DeviceText.normalise(bleState.errorMessage),
              style: AppTheme.statusStyle(AppTheme.red),
            ),
          ],
        ],
      ),
    );
  }
}
