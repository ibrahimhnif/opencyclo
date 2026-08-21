import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:file_picker/file_picker.dart';
import '../../../state/ota_provider.dart';
import '../../theme/app_theme.dart';

class OtaUpdateTab extends ConsumerWidget {
  const OtaUpdateTab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final otaState = ref.watch(otaProvider);
    final otaNotifier = ref.read(otaProvider.notifier);

    return Scaffold(
      appBar: AppBar(
        title: const Text('WIRELESS OTA UPDATE'),
      ),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Info Header Card
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: AppTheme.card,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: AppTheme.cardAccent),
              ),
              child: const Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Icon(Icons.system_update_alt, color: AppTheme.cyan),
                      SizedBox(width: 8),
                      Text(
                        'Over-The-Air (OTA) Flasher',
                        style: TextStyle(
                          color: Colors.white,
                          fontSize: 16,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ],
                  ),
                  SizedBox(height: 8),
                  Text(
                    'Select a compiled firmware binary (.bin) from PlatformIO (.pio/build/esp32-s3-devkitc-1/firmware.bin) to wirelessly flash your OpenCyclo.',
                    style: TextStyle(color: AppTheme.textMuted, fontSize: 13),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 20),

            // File Selector Button
            OutlinedButton.icon(
              style: OutlinedButton.styleFrom(
                padding: const EdgeInsets.symmetric(vertical: 16),
                side: const BorderSide(color: AppTheme.cyan),
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
              ),
              icon: const Icon(Icons.folder_open, color: AppTheme.cyan),
              label: Text(
                otaState.selectedFileName.isNotEmpty
                    ? 'Selected: ${otaState.selectedFileName}'
                    : 'CHOOSE FIRMWARE (.BIN)',
                style: const TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
              ),
              onPressed: (otaState.status == OtaStatus.flashing)
                  ? null
                  : () async {
                      final result = await FilePicker.platform.pickFiles(
                        type: FileType.any,
                        withData: true,
                      );
                      if (result != null && result.files.isNotEmpty) {
                        final file = result.files.first;
                        if (file.bytes != null) {
                          otaNotifier.setFirmwareFile(file.name, file.size, file.bytes!);
                        }
                      }
                    },
            ),
            const SizedBox(height: 24),

            // Flashing Progress Card
            if (otaState.status == OtaStatus.flashing ||
                otaState.status == OtaStatus.success ||
                otaState.status == OtaStatus.error) ...[
              Container(
                padding: const EdgeInsets.all(16),
                decoration: BoxDecoration(
                  color: AppTheme.card,
                  borderRadius: BorderRadius.circular(12),
                  border: Border.all(
                    color: (otaState.status == OtaStatus.success)
                        ? AppTheme.green
                        : (otaState.status == OtaStatus.error)
                            ? AppTheme.red
                            : AppTheme.cyan,
                  ),
                ),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Text(
                      otaState.statusMessage,
                      style: TextStyle(
                        color: (otaState.status == OtaStatus.success)
                            ? AppTheme.green
                            : (otaState.status == OtaStatus.error)
                                ? AppTheme.red
                                : Colors.white,
                        fontWeight: FontWeight.bold,
                      ),
                      textAlign: TextAlign.center,
                    ),
                    const SizedBox(height: 12),
                    LinearProgressIndicator(
                      value: otaState.progress,
                      backgroundColor: AppTheme.heroBg,
                      valueColor: AlwaysStoppedAnimation<Color>(
                        (otaState.status == OtaStatus.success) ? AppTheme.green : AppTheme.cyan,
                      ),
                      minHeight: 8,
                    ),
                    const SizedBox(height: 8),
                    Text(
                      '${(otaState.progress * 100).toStringAsFixed(1)}%',
                      style: const TextStyle(color: AppTheme.textMuted, fontSize: 12),
                      textAlign: TextAlign.center,
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 24),
            ],

            const Spacer(),

            // Flash Start Action Button
            ElevatedButton.icon(
              style: ElevatedButton.styleFrom(
                backgroundColor: AppTheme.green,
                foregroundColor: Colors.black,
                padding: const EdgeInsets.symmetric(vertical: 16),
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
              ),
              icon: otaState.status == OtaStatus.flashing
                  ? const SizedBox(
                      width: 18,
                      height: 18,
                      child: CircularProgressIndicator(strokeWidth: 2, color: Colors.black),
                    )
                  : const Icon(Icons.flash_on),
              label: Text(
                (otaState.status == OtaStatus.flashing)
                    ? 'FLASHING WIRELESSLY...'
                    : 'START WIRELESS FLASH',
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16),
              ),
              onPressed: (otaState.selectedFileName.isEmpty || otaState.status == OtaStatus.flashing)
                  ? null
                  : () => otaNotifier.startFlashing(),
            ),
          ],
        ),
      ),
    );
  }
}
