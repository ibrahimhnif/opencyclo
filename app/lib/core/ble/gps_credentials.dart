import 'package:crypto/crypto.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'gps_registration.dart';

class GpsCredentials {
  final FlutterSecureStorage storage;
  // macOS ad-hoc development builds use the login Keychain (still encrypted,
  // OS access-controlled), not the team-entitlement Data Protection Keychain.
  const GpsCredentials(
      {this.storage = const FlutterSecureStorage(
        mOptions: MacOsOptions(
            usesDataProtectionKeychain: false,
            accountName: 'com.opencyclo.gps.credentials',
            synchronizable: false),
      )});
  // Receiver's silicon ID, never its BLE address or firmware-version string.
  static String key(GpsIdentity id) =>
      'opencyclo.gps.${sha256.convert(id.uniqueId.sublist(10, id.uniqueId.length - 2))}';
  Future<String?> load(GpsIdentity id) => storage.read(key: key(id));
  Future<void> save(GpsIdentity id, String code) async {
    if (code.trim().isEmpty || code.length > 512) {
      throw StateError('Invalid Chipcode');
    }
    await storage.write(key: key(id), value: code.trim());
    if (await storage.read(key: key(id)) != code.trim()) {
      throw StateError('Secure storage verification failed');
    }
  }

  Future<void> forget(GpsIdentity id) => storage.delete(key: key(id));
}
