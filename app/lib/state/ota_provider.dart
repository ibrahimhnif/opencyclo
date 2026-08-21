import 'dart:typed_data';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/ble/ble_service.dart';

enum OtaStatus { idle, selectingFile, flashing, success, error }

class OtaState {
  final OtaStatus status;
  final double progress;
  final String selectedFileName;
  final int selectedFileSize;
  final String statusMessage;

  const OtaState({
    this.status = OtaStatus.idle,
    this.progress = 0.0,
    this.selectedFileName = '',
    this.selectedFileSize = 0,
    this.statusMessage = 'Select a firmware binary (.bin) to begin.',
  });

  OtaState copyWith({
    OtaStatus? status,
    double? progress,
    String? selectedFileName,
    int? selectedFileSize,
    String? statusMessage,
  }) {
    return OtaState(
      status: status ?? this.status,
      progress: progress ?? this.progress,
      selectedFileName: selectedFileName ?? this.selectedFileName,
      selectedFileSize: selectedFileSize ?? this.selectedFileSize,
      statusMessage: statusMessage ?? this.statusMessage,
    );
  }
}

class OtaNotifier extends StateNotifier<OtaState> {
  OtaNotifier() : super(const OtaState());

  Uint8List? _firmwareBytes;

  void setFirmwareFile(String name, int size, Uint8List bytes) {
    _firmwareBytes = bytes;
    state = state.copyWith(
      selectedFileName: name,
      selectedFileSize: size,
      status: OtaStatus.idle,
      progress: 0.0,
      statusMessage: 'Ready to flash $name (${(size / 1024).toStringAsFixed(1)} KB)',
    );
  }

  Future<void> startFlashing() async {
    if (_firmwareBytes == null || _firmwareBytes!.isEmpty) {
      state = state.copyWith(status: OtaStatus.error, statusMessage: 'No firmware file selected');
      return;
    }

    state = state.copyWith(status: OtaStatus.flashing, progress: 0.0, statusMessage: 'Starting OTA flashing...');

    try {
      await for (final progress in BleService.instance.flashFirmware(_firmwareBytes!)) {
        state = state.copyWith(
          progress: progress,
          statusMessage: 'Flashing firmware: ${(progress * 100).toStringAsFixed(1)}%',
        );
      }
      state = state.copyWith(
        status: OtaStatus.success,
        progress: 1.0,
        statusMessage: 'OTA Flashing complete! OpenCyclo is rebooting into the new firmware.',
      );
    } catch (e) {
      state = state.copyWith(status: OtaStatus.error, statusMessage: 'OTA Flashing failed: $e');
    }
  }
}

final otaProvider = StateNotifierProvider<OtaNotifier, OtaState>((ref) {
  return OtaNotifier();
});
