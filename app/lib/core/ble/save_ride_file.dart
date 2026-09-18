import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/foundation.dart';

/// Mobile pickers write the bytes; desktop pickers only return a destination.
Future<String?> saveRideFile(String name, Uint8List bytes) async {
  final desktop = !kIsWeb &&
      (defaultTargetPlatform == TargetPlatform.macOS ||
          defaultTargetPlatform == TargetPlatform.windows ||
          defaultTargetPlatform == TargetPlatform.linux);
  final path = await FilePicker.platform.saveFile(
    fileName: name,
    type: FileType.any,
    bytes: desktop ? null : bytes,
  );
  if (desktop && path != null) {
    await File(path).writeAsBytes(bytes, flush: true);
  }
  return path;
}
