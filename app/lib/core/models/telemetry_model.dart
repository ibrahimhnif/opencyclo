import 'dart:typed_data';

class TelemetryModel {
  final double speedKmh;
  final int cadenceRpm;
  final int heartRateBpm;
  final int powerWatts;
  final double altitudeM;
  final double gradePct;
  final double distanceKm;
  final int rideTimeS;
  final int batteryPct;
  final int gpsFixStatus; // 0=None, 2=3D Fix
  final int satellites;
  final int rideState;    // 0=IDLE, 1=ACTIVE, 2=PAUSED

  const TelemetryModel({
    this.speedKmh = 0.0,
    this.cadenceRpm = -1,
    this.heartRateBpm = -1,
    this.powerWatts = -1,
    this.altitudeM = 0.0,
    this.gradePct = 0.0,
    this.distanceKm = 0.0,
    this.rideTimeS = 0,
    this.batteryPct = 100,
    this.gpsFixStatus = 0,
    this.satellites = 0,
    this.rideState = 0,
  });

  factory TelemetryModel.fromBytes(List<int> bytes) {
    if (bytes.length < 24) return const TelemetryModel();

    final data = ByteData.sublistView(Uint8List.fromList(bytes));

    final int spdRaw = data.getUint16(0, Endian.little);
    final int cadRaw = data.getUint16(2, Endian.little);
    final int hrRaw  = data.getUint16(4, Endian.little);
    final int pwrRaw = data.getUint16(6, Endian.little);
    final int altRaw = data.getInt16(8, Endian.little);
    final int grdRaw = data.getInt16(10, Endian.little);
    final int distRaw = data.getUint32(12, Endian.little);
    final int timeRaw = data.getUint32(16, Endian.little);

    final int bat = bytes[20];
    final int fix = bytes[21];
    final int sat = bytes[22];
    final int state = bytes[23];

    return TelemetryModel(
      speedKmh: spdRaw / 100.0,
      cadenceRpm: (cadRaw == 0xFFFF) ? -1 : cadRaw,
      heartRateBpm: (hrRaw == 0xFFFF) ? -1 : hrRaw,
      powerWatts: (pwrRaw == 0xFFFF) ? -1 : pwrRaw,
      altitudeM: altRaw.toDouble(),
      gradePct: grdRaw / 10.0,
      distanceKm: distRaw / 1000.0,
      rideTimeS: timeRaw,
      batteryPct: bat,
      gpsFixStatus: fix,
      satellites: sat,
      rideState: state,
    );
  }

  String get formattedRideTime {
    final int hrs = rideTimeS ~/ 3600;
    final int mins = (rideTimeS % 3600) ~/ 60;
    final int secs = rideTimeS % 60;
    if (hrs > 0) {
      return '${hrs.toString().padLeft(2, '0')}:${mins.toString().padLeft(2, '0')}:${secs.toString().padLeft(2, '0')}';
    }
    return '${mins.toString().padLeft(2, '0')}:${secs.toString().padLeft(2, '0')}';
  }
}
