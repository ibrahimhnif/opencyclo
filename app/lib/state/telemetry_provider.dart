import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/ble/ble_service.dart';
import '../core/models/telemetry_model.dart';

final telemetryStreamProvider = StreamProvider<TelemetryModel>((ref) {
  return BleService.instance.telemetryStream;
});
