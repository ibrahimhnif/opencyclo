import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../state/telemetry_provider.dart';
import '../../../state/ble_provider.dart';
import '../../../core/models/telemetry_model.dart';
import '../../theme/app_theme.dart';
import '../../widgets/device_ui.dart';

/// Mirrors the device's own "ride" and "climb" pages.
///
/// Slot order is deliberately identical to `TEMPLATE_HERO_6_GRID` as populated
/// in `layout_config.cpp`: speed hero, then distance + ride time across the
/// middle, then cadence / heart / power across the bottom. Someone glancing
/// from the bar-mounted computer to their phone should find the same number in
/// the same place.
class LiveDashboardTab extends ConsumerWidget {
  const LiveDashboardTab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final telemetryAsync = ref.watch(telemetryStreamProvider);
    final data = telemetryAsync.value ?? const TelemetryModel();
    final bleState = ref.watch(bleProvider);
    final linked = bleState.status == DeviceConnectionStatus.connected;

    final hasFix = data.gpsFixStatus >= 2;

    return DeviceScreen(
      statusBar: DeviceStatusBar(
        title: 'live',
        // Segment 2, exactly as renderPage draws it: "gps 12" or "gps --".
        linkText: hasFix ? 'gps ${data.satellites}' : 'gps --',
        linkColor: hasFix ? AppTheme.green : AppTheme.amber,
        // Segment 3: ride state + battery, same strings and colours.
        stateText: linked
            ? '${AppTheme.rideStateText(data.rideState)} ${data.batteryPct}%'
            : 'no link',
        stateColor: linked
            ? AppTheme.rideStateColor(data.rideState)
            : AppTheme.label,
      ),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(
          AppTheme.gutter,
          4,
          AppTheme.gutter,
          24,
        ),
        children: [
          // Slot 0 — the speed hero. The source label mirrors the firmware's
          // "speed . gps" / "speed . ble" exactly.
          DeviceHeroTile(
            label: 'speed . ${linked ? "ble" : "gps"}',
            value: data.speedKmh.toStringAsFixed(1),
            unit: 'km/h',
          ),

          // Slots 1-2 — the middle row. Units ride in the label, per firmware.
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Expanded(
                child: DeviceTile(
                  label: 'dist (km)',
                  value: data.distanceKm.toStringAsFixed(2),
                ),
              ),
              const SizedBox(width: AppTheme.gutter),
              Expanded(
                child: DeviceTile(
                  label: 'ride time',
                  value: data.formattedRideTime,
                ),
              ),
            ],
          ),

          // Slots 3-5 — the bottom row of small tiles. "--" for an absent
          // sensor and a muted value colour, matching renderWidgetCadence
          // and friends.
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Expanded(
                child: DeviceTile(
                  label: 'cadence',
                  value: data.cadenceRpm >= 0 ? '${data.cadenceRpm}' : '--',
                  valueColor:
                      data.cadenceRpm >= 0 ? AppTheme.cyan : AppTheme.label,
                ),
              ),
              const SizedBox(width: AppTheme.gutter),
              Expanded(
                child: DeviceTile(
                  label: 'heart',
                  value: data.heartRateBpm >= 0 ? '${data.heartRateBpm}' : '--',
                  valueColor:
                      data.heartRateBpm >= 0 ? AppTheme.red : AppTheme.label,
                ),
              ),
              const SizedBox(width: AppTheme.gutter),
              Expanded(
                child: DeviceTile(
                  label: 'power',
                  value: data.powerWatts >= 0 ? '${data.powerWatts}' : '--',
                  valueColor:
                      data.powerWatts >= 0 ? AppTheme.green : AppTheme.label,
                ),
              ),
            ],
          ),

          const SizedBox(height: 8),
          const DeviceSectionLabel(text: 'climb'),

          // The device's page 1. Altitude and grade keep their firmware colour
          // rules — grade goes amber climbing, cyan descending, green flat.
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Expanded(
                child: DeviceTile(
                  label: 'alt (m)',
                  value: data.altitudeM.toStringAsFixed(0),
                ),
              ),
              const SizedBox(width: AppTheme.gutter),
              Expanded(
                child: DeviceTile(
                  label: 'grade',
                  value:
                      '${data.gradePct >= 0 ? "+" : ""}${data.gradePct.toStringAsFixed(1)}%',
                  valueColor: AppTheme.gradeColor(data.gradePct),
                ),
              ),
            ],
          ),

          const SizedBox(height: 8),
          const DeviceSectionLabel(text: 'system'),

          DeviceListRow(
            label: 'battery',
            value: '${data.batteryPct}%',
            valueColor: AppTheme.batteryColor(data.batteryPct),
          ),
          DeviceListRow(
            label: 'ride state',
            value: AppTheme.rideStateText(data.rideState),
            valueColor: AppTheme.rideStateColor(data.rideState),
          ),
          DeviceListRow(
            label: 'telemetry link',
            value: linked ? 'streaming' : 'offline',
            valueColor: linked ? AppTheme.green : AppTheme.label,
            showDivider: false,
          ),

          if (!linked) ...[
            const SizedBox(height: 8),
            const DeviceEmptyState(
              text:
                  'no device linked. connect on the device tab to mirror live telemetry.',
            ),
          ],
        ],
      ),
    );
  }
}
