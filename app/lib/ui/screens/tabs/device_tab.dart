import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../state/ble_provider.dart';
import '../../theme/app_theme.dart';

class DeviceTab extends ConsumerWidget {
  const DeviceTab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final bleState = ref.watch(bleProvider);
    final bleNotifier = ref.read(bleProvider.notifier);

    return Scaffold(
      appBar: AppBar(
        title: const Text('OPENCYCLO DEVICE'),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Connection Status Card
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: AppTheme.card,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(
                  color: (bleState.status == DeviceConnectionStatus.connected)
                      ? AppTheme.green
                      : AppTheme.cardAccent,
                ),
              ),
              child: Column(
                children: [
                  Icon(
                    (bleState.status == DeviceConnectionStatus.connected)
                        ? Icons.bluetooth_connected
                        : Icons.bluetooth,
                    size: 48,
                    color: (bleState.status == DeviceConnectionStatus.connected)
                        ? AppTheme.green
                        : AppTheme.cyan,
                  ),
                  const SizedBox(height: 12),
                  Text(
                    (bleState.status == DeviceConnectionStatus.connected)
                        ? 'CONNECTED TO OPENCYCLO'
                        : (bleState.status == DeviceConnectionStatus.connecting)
                            ? 'CONNECTING...'
                            : 'DISCONNECTED',
                    style: TextStyle(
                      fontSize: 16,
                      fontWeight: FontWeight.bold,
                      color: (bleState.status == DeviceConnectionStatus.connected)
                          ? AppTheme.green
                          : Colors.white,
                    ),
                  ),
                  if (bleState.connectedDevice != null) ...[
                    const SizedBox(height: 4),
                    Text(
                      bleState.connectedDevice!.platformName.isNotEmpty
                          ? bleState.connectedDevice!.platformName
                          : bleState.connectedDevice!.remoteId.str,
                      style: const TextStyle(color: AppTheme.textMuted, fontSize: 12),
                    ),
                    const SizedBox(height: 12),
                    ElevatedButton(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: AppTheme.red,
                        foregroundColor: Colors.white,
                      ),
                      onPressed: () => bleNotifier.disconnect(),
                      child: const Text('DISCONNECT'),
                    ),
                  ],
                ],
              ),
            ),
            const SizedBox(height: 20),

            // Scan Action Button
            ElevatedButton.icon(
              style: ElevatedButton.styleFrom(
                backgroundColor: AppTheme.cyan,
                foregroundColor: Colors.black,
                padding: const EdgeInsets.symmetric(vertical: 14),
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
              ),
              icon: bleState.isScanning
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.black),
                    )
                  : const Icon(Icons.search),
              label: Text(
                bleState.isScanning ? 'SCANNING FOR OPENCYCLO...' : 'SCAN FOR DEVICES',
                style: const TextStyle(fontWeight: FontWeight.bold, letterSpacing: 0.5),
              ),
              onPressed: bleState.isScanning
                  ? () => bleNotifier.stopScan()
                  : () => bleNotifier.startScan(),
            ),
            const SizedBox(height: 20),

            // Discovered Devices List
            const Text(
              'DISCOVERED DEVICES',
              style: TextStyle(
                color: AppTheme.textMuted,
                fontSize: 12,
                fontWeight: FontWeight.bold,
                letterSpacing: 1.0,
              ),
            ),
            const SizedBox(height: 8),

            if (bleState.scanResults.isEmpty)
              Container(
                padding: const EdgeInsets.all(24),
                alignment: Alignment.center,
                child: Text(
                  bleState.isScanning
                      ? 'Searching for nearby OpenCyclo computer...'
                      : 'No devices found. Tap "Scan For Devices" to search.',
                  style: const TextStyle(color: AppTheme.textMuted, fontSize: 13),
                  textAlign: TextAlign.center,
                ),
              )
            else
              ListView.builder(
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                itemCount: bleState.scanResults.length,
                itemBuilder: (context, index) {
                  final result = bleState.scanResults[index];
                  final name = result.device.platformName.isNotEmpty
                      ? result.device.platformName
                      : 'Unknown Device';
                  final isOpenCyclo = name.contains('OpenCyclo');

                  return Container(
                    margin: const EdgeInsets.symmetric(vertical: 4),
                    decoration: BoxDecoration(
                      color: AppTheme.card,
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(
                        color: isOpenCyclo ? AppTheme.cyan : AppTheme.cardAccent,
                      ),
                    ),
                    child: ListTile(
                      leading: Icon(
                        Icons.gps_fixed,
                        color: isOpenCyclo ? AppTheme.cyan : AppTheme.textMuted,
                      ),
                      title: Text(
                        name,
                        style: TextStyle(
                          color: isOpenCyclo ? Colors.white : AppTheme.textMuted,
                          fontWeight: isOpenCyclo ? FontWeight.bold : FontWeight.normal,
                        ),
                      ),
                      subtitle: Text(
                        '${result.device.remoteId.str} | RSSI: ${result.rssi} dBm',
                        style: const TextStyle(color: AppTheme.textMuted, fontSize: 11),
                      ),
                      trailing: ElevatedButton(
                        style: ElevatedButton.styleFrom(
                          backgroundColor: AppTheme.green,
                          foregroundColor: Colors.black,
                        ),
                        onPressed: () => bleNotifier.connect(result.device),
                        child: const Text('CONNECT'),
                      ),
                    ),
                  );
                },
              ),
          ],
        ),
      ),
    );
  }
}
