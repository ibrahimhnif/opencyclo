import 'dart:math' show pow;
import 'package:sensors_plus/sensors_plus.dart';

class PhoneBaroSample {
  final double altitudeM;
  const PhoneBaroSample({required this.altitudeM});
}

class PhoneBaroService {
  final Stream<BarometerEvent> Function() _barometerEventStreamFactory;

  const PhoneBaroService(
      {Stream<BarometerEvent> Function()? barometerEventStreamFactory})
      : _barometerEventStreamFactory =
            barometerEventStreamFactory ?? barometerEventStream;

  /// Standard atmosphere formula against a fixed sea-level reference (1013.25
  /// hPa), NOT the device's own calibrated baro reference (see
  /// hardware/baro_calibration.h) -- the firmware only ever falls back to
  /// this when its own barometer/GPS altitude is unusable, or the user has
  /// explicitly forced phone altitude, so an uncalibrated absolute value is
  /// an accepted trade-off rather than a bug.
  Stream<PhoneBaroSample> samples() {
    return _barometerEventStreamFactory().map((event) {
      final altitudeM =
          44330.0 * (1.0 - pow(event.pressure / 1013.25, 1 / 5.255));
      return PhoneBaroSample(altitudeM: altitudeM.toDouble());
    });
  }
}
