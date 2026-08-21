class BleProtocol {
  // Service UUIDs
  static const String devInfoServiceUuid = "180a";
  static const String openCycloServiceUuid = "00001900-0000-1000-8000-00805f9b34fb";
  static const String otaServiceUuid       = "00001910-0000-1000-8000-00805f9b34fb";

  // OpenCyclo Communication Characteristic UUIDs
  static const String layoutConfigCharUuid    = "00001901-0000-1000-8000-00805f9b34fb";
  static const String telemetryStreamCharUuid = "00001902-0000-1000-8000-00805f9b34fb";
  static const String deviceCommandCharUuid   = "00001903-0000-1000-8000-00805f9b34fb";

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
}
