import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo/core/ble/save_ride_file.dart';

class SavePicker extends FilePicker {
  String? destination;
  Uint8List? receivedBytes;

  @override
  Future<String?> saveFile({
    String? dialogTitle,
    String? fileName,
    String? initialDirectory,
    FileType type = FileType.any,
    List<String>? allowedExtensions,
    Uint8List? bytes,
    bool lockParentWindow = false,
  }) async {
    expect(fileName, 'ride.gpx');
    receivedBytes = bytes;
    return destination;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  FilePickerIO.registerWith();
  late Directory directory;
  late FilePicker original;
  late SavePicker picker;
  final bytes = Uint8List.fromList([60, 103, 112, 120, 62, 10, 255]);

  setUp(() async {
    original = FilePicker.platform;
    picker = SavePicker();
    FilePicker.platform = picker;
    directory = await Directory.systemTemp.createTemp('ride-save-test-');
  });
  tearDown(() async {
    FilePicker.platform = original;
    debugDefaultTargetPlatformOverride = null;
    await directory.delete(recursive: true);
  });

  for (final platform in [
    TargetPlatform.macOS,
    TargetPlatform.windows,
    TargetPlatform.linux
  ]) {
    test('$platform saves exact bytes after choosing a destination', () async {
      debugDefaultTargetPlatformOverride = platform;
      picker.destination = '${directory.path}/ride.gpx';
      expect(await saveRideFile('ride.gpx', bytes), picker.destination);
      expect(picker.receivedBytes, isNull);
      expect(await File(picker.destination!).readAsBytes(), bytes);
    });
  }

  test('cancel on macOS does not create a file', () async {
    debugDefaultTargetPlatformOverride = TargetPlatform.macOS;
    expect(await saveRideFile('ride.gpx', bytes), isNull);
    expect(await directory.list().toList(), isEmpty);
  });

  test('desktop write failure is not reported as success', () async {
    debugDefaultTargetPlatformOverride = TargetPlatform.macOS;
    picker.destination = '${directory.path}/missing/ride.gpx';
    await expectLater(
        saveRideFile('ride.gpx', bytes), throwsA(isA<FileSystemException>()));
  });

  for (final platform in [TargetPlatform.android, TargetPlatform.iOS]) {
    test('$platform passes bytes to the native picker', () async {
      debugDefaultTargetPlatformOverride = platform;
      picker.destination = 'content://documents/ride.gpx';
      expect(await saveRideFile('ride.gpx', bytes), picker.destination);
      expect(picker.receivedBytes, bytes);
      expect(await directory.list().toList(), isEmpty);
    });
  }
}
