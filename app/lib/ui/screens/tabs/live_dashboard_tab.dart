import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../state/telemetry_provider.dart';
import '../../../core/models/telemetry_model.dart';
import '../../theme/app_theme.dart';

class LiveDashboardTab extends ConsumerWidget {
  const LiveDashboardTab({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final telemetryAsync = ref.watch(telemetryStreamProvider);
    final data = telemetryAsync.value ?? const TelemetryModel();

    return Scaffold(
      appBar: AppBar(
        title: const Text('LIVE TELEMETRY'),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Hero Speed Card
            Container(
              padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 16),
              decoration: BoxDecoration(
                color: AppTheme.heroBg,
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: AppTheme.cyan),
              ),
              child: Column(
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      const Text(
                        'CURRENT SPEED',
                        style: TextStyle(
                          color: AppTheme.textMuted,
                          fontSize: 12,
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                      Container(
                        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
                        decoration: BoxDecoration(
                          color: (data.gpsFixStatus >= 2) ? AppTheme.green : AppTheme.amber,
                          borderRadius: BorderRadius.circular(4),
                        ),
                        child: Text(
                          (data.gpsFixStatus >= 2) ? 'GPS 3D (${data.satellites} SATS)' : 'NO FIX',
                          style: const TextStyle(color: Colors.black, fontSize: 10, fontWeight: FontWeight.bold),
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Text(
                    data.speedKmh.toStringAsFixed(1),
                    style: const TextStyle(
                      fontSize: 64,
                      fontWeight: FontWeight.bold,
                      color: Colors.white,
                    ),
                  ),
                  const Text(
                    'KM / H',
                    style: TextStyle(color: AppTheme.cyan, fontSize: 14, fontWeight: FontWeight.bold),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 16),

            // Middle Grid: Distance & Ride Time
            Row(
              children: [
                Expanded(
                  child: _MetricCard(
                    label: 'DISTANCE',
                    value: '${data.distanceKm.toStringAsFixed(2)} km',
                    color: Colors.white,
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: _MetricCard(
                    label: 'RIDE TIME',
                    value: data.formattedRideTime,
                    color: Colors.white,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),

            // Sensors Triple Row: Cadence, HR, Power
            Row(
              children: [
                Expanded(
                  child: _MetricCard(
                    label: 'CADENCE',
                    value: (data.cadenceRpm >= 0) ? '${data.cadenceRpm}' : '--',
                    unit: 'RPM',
                    color: AppTheme.cyan,
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: _MetricCard(
                    label: 'HEART RATE',
                    value: (data.heartRateBpm >= 0) ? '${data.heartRateBpm}' : '--',
                    unit: 'BPM',
                    color: AppTheme.red,
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: _MetricCard(
                    label: 'POWER',
                    value: (data.powerWatts >= 0) ? '${data.powerWatts}' : '--',
                    unit: 'W',
                    color: AppTheme.green,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),

            // Climb Row: Altitude & Grade
            Row(
              children: [
                Expanded(
                  child: _MetricCard(
                    label: 'ALTITUDE',
                    value: '${data.altitudeM.toStringAsFixed(0)} m',
                    color: AppTheme.cyan,
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: _MetricCard(
                    label: 'GRADE',
                    value: '${data.gradePct.toStringAsFixed(1)}%',
                    color: (data.gradePct > 3) ? AppTheme.amber : AppTheme.green,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _MetricCard extends StatelessWidget {
  final String label;
  final String value;
  final String? unit;
  final Color color;

  const _MetricCard({
    required this.label,
    required this.value,
    this.unit,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppTheme.card,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppTheme.cardAccent),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            label,
            style: const TextStyle(color: AppTheme.textMuted, fontSize: 10, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 6),
          Row(
            crossAxisAlignment: CrossAxisAlignment.baseline,
            textBaseline: TextBaseline.alphabetic,
            children: [
              Text(
                value,
                style: TextStyle(color: color, fontSize: 20, fontWeight: FontWeight.bold),
              ),
              if (unit != null) ...[
                const SizedBox(width: 4),
                Text(
                  unit!,
                  style: const TextStyle(color: AppTheme.textMuted, fontSize: 10),
                ),
              ],
            ],
          ),
        ],
      ),
    );
  }
}
