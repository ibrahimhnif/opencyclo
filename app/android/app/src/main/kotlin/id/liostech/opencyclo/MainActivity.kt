package id.liostech.opencyclo

import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import android.Manifest
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.location.LocationManager
import android.net.Uri
import android.os.Build
import android.provider.Settings

class MainActivity : FlutterActivity() {
    private var pendingPermission: MethodChannel.Result? = null
    private val permissionRequestCode = 4107
    private val permissionHistory by lazy { getSharedPreferences("ble_access", Context.MODE_PRIVATE) }

    private fun requiredPermissions(): Array<String> = when {
        Build.VERSION.SDK_INT >= 31 -> arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        Build.VERSION.SDK_INT >= 23 -> arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        else -> emptyArray()
    }

    private fun missingPermissions() = requiredPermissions().filter {
        Build.VERSION.SDK_INT >= 23 && checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED
    }

    private fun accessState(): String {
        if (!packageManager.hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) return "unsupported"
        val missing = missingPermissions()
        if (missing.isNotEmpty()) {
            val blocked = Build.VERSION.SDK_INT >= 23 && missing.any {
                permissionHistory.getBoolean(it, false) && !shouldShowRequestPermissionRationale(it)
            }
            return if (blocked) "blocked" else "denied"
        }
        val adapter = (getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
            ?: return "unsupported"
        if (!adapter.isEnabled) return "off"
        // Android 6–11 also requires Location services for BLE discovery.
        if (Build.VERSION.SDK_INT in 23..30) {
            val location = getSystemService(Context.LOCATION_SERVICE) as LocationManager
            val enabled = if (Build.VERSION.SDK_INT >= 28) location.isLocationEnabled else
                location.isProviderEnabled(LocationManager.GPS_PROVIDER) || location.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
            if (!enabled) return "locationOff"
        }
        return "ready"
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, "opencyclo/ble_access")
            .setMethodCallHandler { call, result ->
                try {
                    when (call.method) {
                        "check" -> result.success(accessState())
                        "request" -> {
                            val missing = missingPermissions()
                            val current = accessState()
                            if (pendingPermission != null) {
                                result.error("busy", "Bluetooth permission request already open", null)
                            } else if (missing.isEmpty() || current == "blocked" || current == "unsupported" || Build.VERSION.SDK_INT < 23) {
                                result.success(current)
                            } else {
                                pendingPermission = result
                                requestPermissions(missing.toTypedArray(), permissionRequestCode)
                            }
                        }
                        "settings" -> {
                            val intent = if (accessState() == "locationOff") Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)
                                else Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$packageName"))
                            startActivity(intent)
                            result.success(null)
                        }
                        else -> result.notImplemented()
                    }
                } catch (error: Exception) {
                    pendingPermission = null
                    result.error("ble_access", error.message, null)
                }
            }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != permissionRequestCode) return
        // Only mark completed requests, not interrupted/cancelled activities.
        if (grantResults.isNotEmpty()) {
            permissionHistory.edit().also { editor -> permissions.forEach { editor.putBoolean(it, true) } }.apply()
        }
        val result = pendingPermission
        pendingPermission = null
        try { result?.success(accessState()) }
        catch (error: Exception) { result?.error("ble_access", error.message, null) }
    }
}
