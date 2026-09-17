import 'dart:typed_data';

class BleProtocol {
  // Service UUIDs
  static const String devInfoServiceUuid = "180a";
  static const String openCycloServiceUuid = "00001900-0000-1000-8000-00805f9b34fb";
  static const String otaServiceUuid       = "00001910-0000-1000-8000-00805f9b34fb";

  // OpenCyclo Communication Characteristic UUIDs
  static const String layoutConfigCharUuid    = "00001901-0000-1000-8000-00805f9b34fb";
  static const String telemetryStreamCharUuid = "00001902-0000-1000-8000-00805f9b34fb";
  static const String deviceCommandCharUuid   = "00001903-0000-1000-8000-00805f9b34fb";
  static const String phoneGpsCharUuid        = "0000190a-0000-1000-8000-00805f9b34fb";
  static const String gpsSourceModeCharUuid   = "0000190b-0000-1000-8000-00805f9b34fb";

  // OTA Characteristic UUIDs
  static const String otaControlCharUuid = "00001911-0000-1000-8000-00805f9b34fb";
  static const String otaDataCharUuid    = "00001912-0000-1000-8000-00805f9b34fb";

  // Commands
  static const int cmdStartRide    = 0x01;
  static const int cmdPauseRide    = 0x02;
  static const int cmdResetDefault = 0x03;
  static const int cmdReboot       = 0x04;

  // OTA Commands
  static const int otaCmdBegin = 0x01;
  static const int otaCmdEnd   = 0x02;
  static const int otaCmdAbort = 0x03;

  // GPS Source Modes (0x190B payload)
  static const int gpsSourceHardware    = 0x00;
  static const int gpsSourcePhoneForced = 0x01;
}

/// Encodes a phone position for the 0x190A characteristic: 11 bytes,
/// little-endian `int32 lat_e7, int32 lon_e7, uint16 accuracy_cm, uint8 seq`.
Uint8List encodePhoneGpsSample(double lat, double lon, double accuracyM, int seq) {
  final bytes = ByteData(11);
  bytes.setInt32(0, (lat * 1e7).round(), Endian.little);
  bytes.setInt32(4, (lon * 1e7).round(), Endian.little);
  final accuracyCm = (accuracyM * 100).round().clamp(0, 65535);
  bytes.setUint16(8, accuracyCm, Endian.little);
  bytes.setUint8(10, seq & 0xFF);
  return bytes.buffer.asUint8List();
}
