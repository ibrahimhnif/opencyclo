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
  static const String phoneBaroCharUuid       = "0000190c-0000-1000-8000-00805f9b34fb";
  static const String phoneCompassCharUuid    = "0000190d-0000-1000-8000-00805f9b34fb";
  static const String baroSourceModeCharUuid  = "0000190e-0000-1000-8000-00805f9b34fb";
  static const String compassSourceModeCharUuid = "0000190f-0000-1000-8000-00805f9b34fb";

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

  // Baro/Compass Source Modes (0x190E/0x190F payload) -- same convention.
  static const int altitudeSourceHardware    = 0x00;
  static const int altitudeSourcePhoneForced = 0x01;
  static const int headingSourceHardware     = 0x00;
  static const int headingSourcePhoneForced  = 0x01;
}

/// Encodes a phone position for the 0x190A characteristic: 15 bytes,
/// little-endian `int32 lat_e7, int32 lon_e7, uint16 accuracy_cm, uint8 seq,
/// uint32 utc_epoch_s`. `utcEpochS` is the phone clock's UTC seconds since
/// 1970-01-01 at the time of the fix -- the firmware uses it to timestamp
/// GPX trackpoints when the onboard GPS has never had its own UTC fix this
/// boot (e.g. phone-forced mode with no hardware fix at all).
Uint8List encodePhoneGpsSample(
    double lat, double lon, double accuracyM, int seq, int utcEpochS) {
  final bytes = ByteData(15);
  bytes.setInt32(0, (lat * 1e7).round(), Endian.little);
  bytes.setInt32(4, (lon * 1e7).round(), Endian.little);
  final accuracyCm = (accuracyM * 100).round().clamp(0, 65535);
  bytes.setUint16(8, accuracyCm, Endian.little);
  bytes.setUint8(10, seq & 0xFF);
  bytes.setUint32(11, utcEpochS, Endian.little);
  return bytes.buffer.asUint8List();
}

/// Encodes a phone altitude for the 0x190C characteristic: 3 bytes,
/// little-endian `int16 altitude_dm, uint8 seq`.
Uint8List encodePhoneAltitudeSample(double altitudeM, int seq) {
  final bytes = ByteData(3);
  final altitudeDm = (altitudeM * 10).round().clamp(-32768, 32767);
  bytes.setInt16(0, altitudeDm, Endian.little);
  bytes.setUint8(2, seq & 0xFF);
  return bytes.buffer.asUint8List();
}

/// Encodes a phone compass heading for the 0x190D characteristic: 4 bytes,
/// little-endian `uint16 heading_deci_deg, uint8 accuracy, uint8 seq`.
/// `accuracy` is 0=low, 1=medium, 2=high, matching the firmware's convention.
Uint8List encodePhoneHeadingSample(double headingDeg, int accuracy, int seq) {
  final bytes = ByteData(4);
  final deciDeg = (headingDeg * 10).round() % 3600;
  bytes.setUint16(0, deciDeg < 0 ? deciDeg + 3600 : deciDeg, Endian.little);
  bytes.setUint8(2, accuracy & 0xFF);
  bytes.setUint8(3, seq & 0xFF);
  return bytes.buffer.asUint8List();
}
