import 'package:flutter_compass/flutter_compass.dart';

class PhoneCompassSample {
  final double headingDeg;
  /// 0=low, 1=medium, 2=high -- matches the firmware's convention.
  final int accuracy;
  const PhoneCompassSample({required this.headingDeg, required this.accuracy});
}

class PhoneCompassService {
  final Stream<CompassEvent>? Function() _eventsFactory;

  const PhoneCompassService({Stream<CompassEvent>? Function()? eventsFactory})
      : _eventsFactory = eventsFactory ?? _defaultEvents;

  static Stream<CompassEvent>? _defaultEvents() => FlutterCompass.events;

  /// True when the platform exposes a magnetometer at all -- `null` means no
  /// compass hardware, distinct from a heading sample simply not having
  /// arrived yet.
  bool get isSupported => _eventsFactory() != null;

  /// [CompassEvent.accuracy] is a plus/minus degree error (reliable on iOS,
  /// partly hardcoded on Android per flutter_compass's own docs) -- bucketed
  /// into the firmware's 0/1/2 scale rather than passed through raw, and
  /// defaulted to medium when the platform doesn't report one at all.
  static int _bucketAccuracy(double? accuracyDeg) {
    if (accuracyDeg == null) return 1;
    if (accuracyDeg <= 15) return 2;
    if (accuracyDeg <= 35) return 1;
    return 0;
  }

  Stream<PhoneCompassSample> samples() {
    final events = _eventsFactory();
    if (events == null) return const Stream.empty();
    return events
        .where((e) => e.heading != null)
        .map((e) => PhoneCompassSample(
              headingDeg: e.heading!,
              accuracy: _bucketAccuracy(e.accuracy),
            ));
  }
}
