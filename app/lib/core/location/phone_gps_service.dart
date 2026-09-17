import 'package:geolocator/geolocator.dart';

typedef PositionStreamFactory = Stream<Position> Function();

class PhoneGpsSample {
  final double latitude;
  final double longitude;
  final double accuracyM;
  const PhoneGpsSample({
    required this.latitude,
    required this.longitude,
    required this.accuracyM,
  });
}

class PhoneGpsService {
  final PositionStreamFactory _positionStreamFactory;

  const PhoneGpsService({PositionStreamFactory? positionStreamFactory})
      : _positionStreamFactory = positionStreamFactory ?? _defaultPositionStream;

  static Stream<Position> _defaultPositionStream() {
    return Geolocator.getPositionStream(
      locationSettings: const LocationSettings(
        accuracy: LocationAccuracy.best,
        distanceFilter: 0,
      ),
    );
  }

  Stream<PhoneGpsSample> positions() {
    return _positionStreamFactory().map((p) => PhoneGpsSample(
          latitude: p.latitude,
          longitude: p.longitude,
          accuracyM: p.accuracy,
        ));
  }
}
