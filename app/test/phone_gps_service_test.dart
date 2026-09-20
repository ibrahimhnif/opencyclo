import 'package:flutter_test/flutter_test.dart';
import 'package:geolocator/geolocator.dart';
import 'package:opencyclo/core/location/phone_gps_service.dart';

Position _fakePosition(double lat, double lon, double accuracy) {
  return Position(
    latitude: lat,
    longitude: lon,
    timestamp: DateTime.now(),
    accuracy: accuracy,
    altitude: 0,
    altitudeAccuracy: 0,
    heading: 0,
    headingAccuracy: 0,
    speed: 0,
    speedAccuracy: 0,
  );
}

void main() {
  test('positions() maps Geolocator Position into PhoneGpsSample', () async {
    final service = PhoneGpsService(
      positionStreamFactory: () => Stream.fromIterable(
        [_fakePosition(37.7749, -122.4194, 8.0)],
      ),
    );

    final sample = await service.positions().first;
    expect(sample.latitude, 37.7749);
    expect(sample.longitude, -122.4194);
    expect(sample.accuracyM, 8.0);
  });
}
