import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:file_picker/file_picker.dart';
import '../../../state/ota_provider.dart';
import '../../../state/ble_provider.dart';
import '../../theme/app_theme.dart';
import '../../widgets/device_ui.dart';

/// Wireless flashing, in the device's voice.
///
/// The progress bar is a hairline track filling with a status colour — the same
/// visual grammar as the device's own coloured status segments, rather than a
/// Material card with a rounded indicator.
class OtaUpdateTab extends ConsumerWidget {
  const OtaUpdateTab({super.key});

  static String _formatBytes(int bytes) {
    if (bytes <= 0) return '0 kb';
    if (bytes < 1024) return '$bytes b';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(0)} kb';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(2)} mb';
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final otaState = ref.watch(otaProvider);
    final otaNotifier = ref.read(otaProvider.notifier);
    final bleState = ref.watch(bleProvider);

    final linked = bleState.status == DeviceConnectionStatus.connected;
    final flashing = otaState.status == OtaStatus.flashing;
    final hasFile = otaState.selectedFileName.isNotEmpty;

    final Color statusColor;
    switch (otaState.status) {
      case OtaStatus.success:
        statusColor = AppTheme.green;
      case OtaStatus.error:
        statusColor = AppTheme.red;
      case OtaStatus.flashing:
        statusColor = AppTheme.amber;
      case OtaStatus.idle:
      case OtaStatus.selectingFile:
        statusColor = AppTheme.label;
    }

    return DeviceScreen(
      statusBar: DeviceStatusBar(
        title: 'ota',
        linkText: linked ? 'linked' : 'no link',
        linkColor: linked ? AppTheme.green : AppTheme.amber,
        stateText: flashing ? 'flashing' : null,
        stateColor: AppTheme.amber,
      ),
      bottomAction: DeviceButton(
        text: flashing ? 'flashing wirelessly...' : 'start wireless flash',
        color: flashing ? AppTheme.amber : AppTheme.green,
        icon: Icons.flash_on,
        busy: flashing,
        onPressed: (!hasFile || flashing) ? null : otaNotifier.startFlashing,
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(
          AppTheme.gutter,
          8,
          AppTheme.gutter,
          24,
        ),
        children: [
          const DeviceSectionLabel(text: 'firmware'),

          DeviceListRow(
            label: 'binary',
            value: hasFile ? otaState.selectedFileName : 'none selected',
            valueColor: hasFile ? AppTheme.text : AppTheme.label,
          ),
          DeviceListRow(
            label: 'size',
            value: hasFile ? _formatBytes(otaState.selectedFileSize) : '--',
            valueColor: hasFile ? AppTheme.text : AppTheme.label,
          ),
          DeviceListRow(
            label: 'target',
            value: linked ? 'opencyclo' : 'not linked',
            valueColor: linked ? AppTheme.green : AppTheme.amber,
            showDivider: false,
          ),

          const SizedBox(height: 16),
          DeviceButton(
            text: hasFile ? 'choose another binary' : 'choose firmware (.bin)',
            color: AppTheme.cyan,
            icon: Icons.folder_open,
            outlined: true,
            onPressed: flashing
                ? null
                : () async {
                    final result = await FilePicker.platform.pickFiles(
                      type: FileType.any,
                      withData: true,
                    );
                    if (result == null || result.files.isEmpty) return;
                    final file = result.files.first;
                    if (file.bytes != null) {
                      otaNotifier.setFirmwareFile(
                        file.name,
                        file.size,
                        file.bytes!,
                      );
                    }
                  },
          ),

          const SizedBox(height: 24),
          const DeviceSectionLabel(text: 'progress'),

          // Hairline track filling with the status colour, mirroring how the
          // device signals state through colour alone.
          ClipRRect(
            borderRadius: BorderRadius.circular(1),
            child: LinearProgressIndicator(
              value: otaState.progress,
              minHeight: 3,
              backgroundColor: AppTheme.hairline,
              valueColor: AlwaysStoppedAnimation<Color>(
                otaState.status == OtaStatus.error
                    ? AppTheme.red
                    : otaState.status == OtaStatus.success
                        ? AppTheme.green
                        : AppTheme.cyan,
              ),
            ),
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: Text(
                  DeviceText.normalise(otaState.statusMessage),
                  style: AppTheme.statusStyle(statusColor),
                ),
              ),
              const SizedBox(width: 12),
              Text(
                '${(otaState.progress * 100).toStringAsFixed(1)}%',
                style: AppTheme.valueStyle(statusColor).copyWith(fontSize: 14),
              ),
            ],
          ),

          const SizedBox(height: 28),
          const DeviceSectionLabel(text: 'notes'),
          Text(
            'select a compiled binary from '
            '.pio/build/esp32-s3-devkitc-1/firmware.bin. keep the device awake '
            'and in range for the whole transfer — a dropped link mid-flash '
            'leaves the device on its previous image.',
            style: AppTheme.bodyStyle,
          ),

          if (!linked) ...[
            const SizedBox(height: 16),
            Text(
              'no ble link. connect on the device tab before flashing.',
              style: AppTheme.statusStyle(AppTheme.amber),
            ),
          ],
        ],
      ),
    );
  }
}
