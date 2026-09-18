import 'dart:io' show Platform;
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:geolocator/geolocator.dart';

typedef PositionStreamFactory = Stream<Position> Function();

class PhoneGpsSample {
  final double latitude;
  final double longitude;
  final double accuracyM;
  final DateTime timestamp;
  const PhoneGpsSample({
    required this.latitude,
    required this.longitude,
    required this.accuracyM,
    required this.timestamp,
  });
}

class PhoneGpsService {
  final PositionStreamFactory _positionStreamFactory;

  const PhoneGpsService({PositionStreamFactory? positionStreamFactory})
      : _positionStreamFactory = positionStreamFactory ?? _defaultPositionStream;

  static Stream<Position> _defaultPositionStream() {
    // The device is fed from a pocket/mount with the screen off, so iOS needs
    // background location updates explicitly enabled (Info.plist already
    // declares the `location` background mode), and Android needs a steady
    // ~1 Hz interval rather than the platform default.
    final LocationSettings settings = !kIsWeb && Platform.isIOS
        ? AppleSettings(
            accuracy: LocationAccuracy.best,
            distanceFilter: 0,
            pauseLocationUpdatesAutomatically: false,
            showBackgroundLocationIndicator: true,
            allowBackgroundLocationUpdates: true,
          )
        : !kIsWeb && Platform.isAndroid
            ? AndroidSettings(
                accuracy: LocationAccuracy.best,
                distanceFilter: 0,
                forceLocationManager: false,
                intervalDuration: const Duration(seconds: 1),
              )
            : const LocationSettings(
                accuracy: LocationAccuracy.best,
                distanceFilter: 0,
              );
    return Geolocator.getPositionStream(locationSettings: settings);
  }

  /// Makes sure location services are on and the app holds a usable location
  /// permission, requesting it once if it has not been asked for yet.
  ///
  /// Returns false when the position stream cannot be started -- the caller is
  /// expected to surface that rather than silently produce no samples.
  Future<bool> ensurePermission() async {
    if (!await Geolocator.isLocationServiceEnabled()) return false;

    LocationPermission permission = await Geolocator.checkPermission();
    if (permission == LocationPermission.denied) {
      permission = await Geolocator.requestPermission();
    }
    return permission == LocationPermission.always ||
        permission == LocationPermission.whileInUse;
  }

  Stream<PhoneGpsSample> positions() {
    return _positionStreamFactory().map((p) => PhoneGpsSample(
          latitude: p.latitude,
          longitude: p.longitude,
          accuracyM: p.accuracy,
          timestamp: p.timestamp,
        ));
  }
}
