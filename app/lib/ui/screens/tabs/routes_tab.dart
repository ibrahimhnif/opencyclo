import 'dart:convert';
import 'dart:math' as math;
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:flutter/foundation.dart';
import '../../../core/ble/ble_service.dart';
import '../../../core/models/route_model.dart';
import '../../theme/app_theme.dart';

RouteModel _parseRoute(Map<String, String> input) =>
    RouteModel.fromGpx(input['text']!, name: input['name']!);

class RoutesTab extends StatefulWidget {
  final RouteModel? initialRoute;
  const RoutesTab({super.key, this.initialRoute});
  @override
  State<RoutesTab> createState() => _RoutesTabState();
}

class _RoutesTabState extends State<RoutesTab> {
  RouteModel? route;
  String status = 'import a GPX to preview and sync';
  String coverage = 'map coverage not checked';
  bool busy = false, cancel = false, synced = false;
  double progress = 0;
  @override
  void initState() {
    super.initState();
    route = widget.initialRoute;
  }

  Future<void> run(Future<void> Function() operation) async {
    setState(() => busy = true);
    try {
      await operation();
    } catch (e) {
      if (mounted) setState(() => status = e.toString());
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  @override
  void dispose() {
    cancel = true;
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => SafeArea(
          child: ListView(padding: const EdgeInsets.all(16), children: [
        Text('routes', style: Theme.of(context).textTheme.headlineMedium),
        const SizedBox(height: 12),
        const Text(
            'free ride follows your GPS position. import a GPX for route navigation.'),
        const SizedBox(height: 12),
        FilledButton(
            onPressed: busy
                ? null
                : () => run(() async {
                      await BleService.instance.freeRide();
                      if (mounted) {
                        setState(() => status = 'free ride opened on device');
                      }
                    }),
            child: const Text('open free ride')),
        OutlinedButton(
            onPressed: busy
                ? null
                : () => run(() async {
                      final files = await FilePicker.platform.pickFiles(
                          type: FileType.custom,
                          allowedExtensions: ['gpx'],
                          withData: true);
                      if (files == null) return;
                      final file = files.files.single;
                      if (file.size > 8 * 1024 * 1024) {
                        throw const FormatException('GPX exceeds 8 MB');
                      }
                      if (file.bytes == null) {
                        throw const FormatException('Could not read GPX');
                      }
                      final imported = await compute(_parseRoute, {
                        'text': utf8.decode(file.bytes!),
                        'name': file.name
                      });
                      if (mounted) {
                        setState(() {
                          route = imported;
                          synced = false;
                          progress = 0;
                          status = 'route ready';
                          coverage = 'map coverage not checked';
                        });
                      }
                    }),
            child: const Text('import GPX')),
        if (route != null) ...[
          const SizedBox(height: 12),
          Text(route!.name),
          Text(
              '${(route!.distance / 1000).toStringAsFixed(1)} km · ${route!.points.length} points · ${route!.cues.length} cues'),
          SizedBox(
              height: 220, child: CustomPaint(painter: _RoutePainter(route!))),
          const Text(
              'preview: route geometry only. copy the offline map pack to the device SD separately.'),
          Text(coverage),
          const SizedBox(height: 8),
          FilledButton(
              onPressed: busy
                  ? null
                  : () => run(() async {
                        cancel = false;
                        synced = false;
                        await BleService.instance.syncRoute(route!, (p) {
                          if (mounted) setState(() => progress = p);
                        }, cancelled: () => cancel);
                        if (mounted) {
                          setState(() {
                            synced = true;
                            status = 'saved to device SD';
                          });
                        }
                        final result =
                            await BleService.instance.routeMapCoverage(route!);
                        if (mounted) {
                          setState(() => coverage = result == 'MAP covered'
                              ? 'offline map covers this route'
                              : result == 'MAP missing'
                                  ? 'offline map missing for part of this route'
                                  : 'map coverage unknown; install the map pack');
                        }
                      }),
              child: const Text('sync route to device')),
          if (busy) ...[
            LinearProgressIndicator(value: progress),
            TextButton(
                onPressed: () => setState(() => cancel = true),
                child: const Text('cancel transfer'))
          ],
          OutlinedButton(
              onPressed: busy || !synced
                  ? null
                  : () => run(() async {
                        await BleService.instance.selectRoute(route!);
                        if (mounted) {
                          setState(
                              () => status = 'navigation opened on device');
                        }
                      }),
              child: const Text('start GPX navigation')),
          const SizedBox(height: 8),
          const Text(
              '“route bends” cues are estimated from the GPX shape, not verified intersection instructions.'),
          for (final cue in route!.cues)
            ListTile(
                contentPadding: EdgeInsets.zero,
                leading: Icon(cue.direction < 0
                    ? Icons.turn_left
                    : cue.direction > 0
                        ? Icons.turn_right
                        : Icons.navigation),
                title: Text(cue.text),
                subtitle: Text(
                    '${(route!.distances[cue.point] / 1000).toStringAsFixed(2)} km')),
        ],
        const SizedBox(height: 12),
        Text(status, style: const TextStyle(color: AppTheme.cyan)),
      ]));
}

class _RoutePainter extends CustomPainter {
  final RouteModel route;
  _RoutePainter(this.route);
  @override
  void paint(Canvas canvas, Size size) {
    final cos = math.cos(route.points.first.lat * math.pi / 180);
    final xs = route.points.map((p) => p.lon * cos).toList(),
        ys = route.points.map((p) => -p.lat).toList();
    final minX = xs.reduce(math.min),
        maxX = xs.reduce(math.max),
        minY = ys.reduce(math.min),
        maxY = ys.reduce(math.max);
    final scale = math.min((size.width - 24) / math.max(maxX - minX, 0.000001),
        (size.height - 24) / math.max(maxY - minY, 0.000001));
    Offset point(int i) => Offset(
        (xs[i] - (minX + maxX) / 2) * scale + size.width / 2,
        (ys[i] - (minY + maxY) / 2) * scale + size.height / 2);
    final path = Path()..moveTo(point(0).dx, point(0).dy);
    for (var i = 1; i < xs.length; i++) {
      path.lineTo(point(i).dx, point(i).dy);
    }
    canvas.drawPath(
        path,
        Paint()
          ..color = AppTheme.cyan
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2);
    canvas.drawCircle(point(0), 5, Paint()..color = AppTheme.green);
    canvas.drawCircle(point(xs.length - 1), 4, Paint()..color = AppTheme.red);
  }

  @override
  bool shouldRepaint(_RoutePainter old) => old.route != route;
}
