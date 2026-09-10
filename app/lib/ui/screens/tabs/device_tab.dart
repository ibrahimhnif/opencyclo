import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../state/ble_provider.dart';
import '../../theme/app_theme.dart';
import '../../widgets/device_ui.dart';

/// Mirrors `renderWidgetBleManager` on the device.
///
/// Same structure, top to bottom: a full-width scan button that turns amber
/// while scanning, then a list of sensor rows carrying a mac address and a
/// destructive chip, then a muted status footer. The device says "scanning..."
/// and "not paired"; so does this.
class DeviceTab extends ConsumerWidget {
  const DeviceTab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final bleState = ref.watch(bleProvider);
    final bleNotifier = ref.read(bleProvider.notifier);

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
          // The device draws its scan button first, cyan when idle and amber
          // while a scan is running. Same colour logic here.
          DeviceButton(
            text: bleState.isScanning ? 'scanning...' : 'scan',
            color: bleState.isScanning ? AppTheme.amber : AppTheme.cyan,
            icon: Icons.search,
            busy: bleState.isScanning,
            onPressed: bleState.isScanning
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
                  onTap: () => bleNotifier.connect(result.device),
                ),
              );
            }),

          const SizedBox(height: 16),

          // The device closes its BLE page with a muted one-line status. So
          // does this — same voice, same colour.
          Text(
            connected
                ? 'link: gatt connected, telemetry streaming'
                : 'link: searching...',
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
