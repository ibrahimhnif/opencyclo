import 'package:flutter/material.dart';
import '../../core/models/layout_config_model.dart';
import '../theme/app_theme.dart';

/// A 240x320 layout preview with firmware-matched slots and action placement.
/// Uses bundled Arimo rather than ESP32 bitmap fonts; not pixel-identical.
///
/// Slot rectangles are transcribed verbatim from `s_templates[]` in
/// `src/ui/engine/template_engine.cpp`, so the preview shows the real geometry
/// the firmware will use — not an approximation. If a template's rects change
/// there, change them here.
class DevicePreview extends StatefulWidget {
  final PageConfigModel page;

  /// Drawn muted in the header's first segment, like `page.title`.
  final String title;

  final int pageIndex;
  final int pageCount;

  const DevicePreview({
    super.key,
    required this.page,
    required this.title,
    required this.pageIndex,
    required this.pageCount,
  });

  @override
  State<DevicePreview> createState() => _DevicePreviewState();
}

class _DevicePreviewState extends State<DevicePreview> {
  bool cameraOptions = false;
  PageConfigModel get page => widget.page;
  String get title => widget.title;
  int get pageIndex => widget.pageIndex;
  int get pageCount => widget.pageCount;
  @override
  void didUpdateWidget(covariant DevicePreview oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.pageIndex != widget.pageIndex) cameraOptions = false;
  }

  static const double _panelW = 240;
  static const double _panelH = 320;

  /// Verbatim from template_engine.cpp.
  static const Map<LayoutTemplate, List<Rect>> _slots = {
    LayoutTemplate.hero6Grid: [
      Rect.fromLTWH(4, 44, 232, 78),
      Rect.fromLTWH(4, 126, 114, 60),
      Rect.fromLTWH(122, 126, 114, 60),
      Rect.fromLTWH(4, 190, 74, 54),
      Rect.fromLTWH(83, 190, 74, 54),
      Rect.fromLTWH(162, 190, 74, 54),
    ],
    LayoutTemplate.fourGrid: [
      Rect.fromLTWH(4, 44, 114, 114),
      Rect.fromLTWH(122, 44, 114, 114),
      Rect.fromLTWH(4, 164, 114, 134),
      Rect.fromLTWH(122, 164, 114, 134),
    ],
    LayoutTemplate.twoGridChart: [
      Rect.fromLTWH(4, 44, 114, 58),
      Rect.fromLTWH(122, 44, 114, 58),
      Rect.fromLTWH(4, 106, 232, 192),
    ],
    LayoutTemplate.eightGrid: [
      Rect.fromLTWH(4, 44, 114, 48),
      Rect.fromLTWH(122, 44, 114, 48),
      Rect.fromLTWH(4, 96, 114, 64),
      Rect.fromLTWH(122, 96, 114, 64),
      Rect.fromLTWH(4, 164, 114, 64),
      Rect.fromLTWH(122, 164, 114, 64),
      Rect.fromLTWH(4, 232, 114, 64),
      Rect.fromLTWH(122, 232, 114, 64),
    ],
    LayoutTemplate.fullContainer: [
      Rect.fromLTWH(4, 44, 232, 258),
    ],
  };

  /// `has_action_button` is true only for the hero 6-grid, whose button rect is
  /// {4, 248, 232, 50}.
  static const Rect _actionButton = Rect.fromLTWH(4, 248, 232, 50);

  /// A plausible value and the firmware's own value colour for each widget, so
  /// the preview reads as a real screen rather than a wireframe. The label comes
  /// straight from the catalog (`WidgetType.label`), and the unit is appended in
  /// parentheses exactly as the device does it — see the "unit rides in the
  /// label" comments in widget_registry.cpp.
  static (String, String, Color) _widgetFace(WidgetType w) {
    switch (w) {
      case WidgetType.speed:
        return ('speed . gps', '24.6', AppTheme.text);
      case WidgetType.avgSpeed:
        return (w.label, '21.4', AppTheme.text);
      case WidgetType.maxSpeed:
        return (w.label, '48.2', AppTheme.text);
      case WidgetType.distance:
        return ('${w.label} (km)', '32.75', AppTheme.text);
      case WidgetType.rideTime:
        return (w.label, '01:12:40', AppTheme.text);
      case WidgetType.cadence:
        return (w.label, '88', AppTheme.cyan);
      case WidgetType.heartRate:
        return (w.label, '146', AppTheme.red);
      case WidgetType.power:
        return (w.label, '212', AppTheme.green);
      case WidgetType.altitude:
        return ('${w.label} (m)', '412', AppTheme.text);
      case WidgetType.grade:
        return (w.label, '+4.2%', AppTheme.amber);
      case WidgetType.totalAscent:
        return ('${w.label} (m)', '648', AppTheme.green);
      case WidgetType.elevationChart:
        return (w.label, '', AppTheme.green);
      case WidgetType.battery:
        return (w.label, '82%', AppTheme.green);
      case WidgetType.cameraRemote:
        return ('Insta360', '', AppTheme.text);
      case WidgetType.bleManager:
        return (w.label, '', AppTheme.text);
      case WidgetType.settingsList:
        return (w.label, '', AppTheme.text);
      case WidgetType.none:
        return ('', '', AppTheme.label);
    }
  }

  static TextStyle _font(double size, Color color, FontWeight weight) =>
      TextStyle(
        fontFamily: AppTheme.fontFamily,
        fontSize: size,
        color: color,
        fontWeight: weight,
        height: 1.15,
      );

  @override
  Widget build(BuildContext context) {
    final rects = _slots[page.template] ?? const <Rect>[];
    final hasAction = page.template == LayoutTemplate.hero6Grid;

    return LayoutBuilder(
      builder: (context, constraints) {
        // Scale the whole 240x320 panel to whatever width we're given, so the
        // preview stays pixel-proportional to the real display.
        final scale = constraints.maxWidth / _panelW;

        return SizedBox(
          width: constraints.maxWidth,
          height: _panelH * scale,
          child: FittedBox(
            fit: BoxFit.fill,
            child: Container(
              width: _panelW,
              height: _panelH,
              color: AppTheme.bg,
              child: Stack(
                children: [
                  _statusHeader(),
                  for (var i = 0; i < rects.length; i++)
                    _slot(
                      rects[i],
                      i < page.widgets.length
                          ? page.widgets[i]
                          : WidgetType.none,
                      isHero:
                          page.template == LayoutTemplate.hero6Grid && i == 0,
                    ),
                  if (hasAction) _action(),
                  _dots(),
                ],
              ),
            ),
          ),
        );
      },
    );
  }

  Widget _text(String text, double x, double y, double width,
          {double size = 18, Color color = AppTheme.text}) =>
      Positioned(
          left: x,
          top: y,
          width: width,
          height: size + 4,
          child: ClipRect(
              child: Text(text,
                  maxLines: 1,
                  softWrap: false,
                  overflow: TextOverflow.clip,
                  style: _font(size, color, FontWeight.w700)
                      .copyWith(height: 1))));

  Widget _statusHeader() => Positioned(
      left: 0,
      top: 0,
      width: 240,
      height: 44,
      child: Stack(children: [
        const Positioned(
            left: 10,
            top: 10,
            child: Icon(Icons.map_outlined, size: 24, color: AppTheme.cyan)),
        _text(title, 48, 12, 144),
        const Positioned(
            left: 206,
            top: 10,
            child: Icon(Icons.directions_bike, size: 24, color: AppTheme.cyan)),
      ]));

  Widget _button(String label, IconData icon, double x, double y, double w,
          {Color bg = const Color(0xFF161A20),
          Color fg = AppTheme.text,
          VoidCallback? onTap}) =>
      Positioned(
          left: x,
          top: y,
          width: w,
          height: 48,
          child: GestureDetector(
              onTap: onTap,
              child: Container(
                  decoration: BoxDecoration(
                      color: bg, borderRadius: BorderRadius.circular(8)),
                  child: Stack(children: [
                    Positioned(
                        left: 8,
                        top: 12,
                        child: Icon(icon, size: 24, color: fg)),
                    _text(label, 38, 15, w - 42, color: fg),
                  ]))));

  Widget _management(WidgetType type, double width) {
    if (type == WidgetType.cameraRemote) {
      final half = (width - 24) / 2;
      return Stack(children: [
        _text('Connected', 8, 8, width - 16, color: AppTheme.green),
        if (!cameraOptions) ...[
          _button('Pair', Icons.search, 8, 40, width - 16),
          _button('Shutter', Icons.camera_alt_outlined, 8, 98, width - 16,
              bg: AppTheme.cyan, fg: AppTheme.bg),
          _button('Options', Icons.list, 8, 156, width - 16,
              onTap: () => setState(() => cameraOptions = true)),
        ] else ...[
          _button('Back', Icons.chevron_left, 8, 40, width - 16,
              onTap: () => setState(() => cameraOptions = false)),
          _button('Mode', Icons.videocam_outlined, 8, 100, half),
          _button(
              'Screen', Icons.desktop_windows_outlined, 16 + half, 100, half),
          _button('Wake', Icons.bolt, 8, 158, half),
          _button('Off', Icons.power_settings_new, 16 + half, 158, half,
              fg: AppTheme.amber),
        ],
        _text(cameraOptions ? 'Camera controls' : 'Shutter follows mode', 8,
            226, width - 16,
            color: AppTheme.label),
      ]);
    }
    if (type == WidgetType.settingsList) {
      const labels = [
        'units',
        'brightness',
        'wheel size',
        'sd logging',
        'battery',
        'firmware'
      ];
      const values = [
        'metric',
        '74%',
        '2096 mm',
        'enabled',
        '82% (4.00V)',
        'v0.2.0'
      ];
      return Stack(children: [
        for (int i = 0; i < labels.length; i++) ...[
          _text(labels[i], 10, 8 + i * 32, 104, color: AppTheme.label),
          _text(values[i], 116, 8 + i * 32, width - 120),
          Positioned(
              left: 6,
              top: 28 + i * 32,
              width: width - 12,
              height: 1,
              child: const ColoredBox(color: AppTheme.hairline)),
        ],
        _button('Power', Icons.power_settings_new, 10, 208, width - 20,
            bg: AppTheme.cyan, fg: AppTheme.bg),
      ]);
    }
    return Stack(children: [
      _button('Scan', Icons.search, 6, 6, width - 12,
          bg: AppTheme.cyan, fg: AppTheme.bg),
      for (int i = 0; i < 3; i++) ...[
        _text(['Speed', 'Heart', 'Power'][i], 10, 64 + i * 52, 120),
        _text('Not paired', 10, 87 + i * 52, width - 20, color: AppTheme.label),
      ],
      _text('GPS ready', 10, 230, width - 20, color: AppTheme.label),
    ]);
  }

  Widget _slot(Rect r, WidgetType w, {required bool isHero}) {
    final (label, value, color) = _widgetFace(w);
    final management = [
      WidgetType.cameraRemote,
      WidgetType.settingsList,
      WidgetType.bleManager
    ].contains(w);
    return Positioned(
        left: r.left,
        top: r.top,
        width: r.width,
        height: r.height,
        child: ClipRect(
            child: w == WidgetType.none
                ? const SizedBox.shrink()
                : management
                    ? _management(w, r.width)
                    : Stack(children: [
                        if (!isHero)
                          Positioned(
                              left: 0,
                              top: 0,
                              width: r.width,
                              height: 1,
                              child:
                                  const ColoredBox(color: AppTheme.hairline)),
                        _text(label, 4, 4, r.width - 8, color: AppTheme.label),
                        if (w != WidgetType.elevationChart)
                          _text(value, 4, 24, r.width - 8,
                              size: isHero ? 48 : 24, color: color),
                        if (isHero)
                          _text('km/h', r.width - 44, r.height - 18, 44,
                              color: AppTheme.label),
                        if (w == WidgetType.elevationChart)
                          Positioned(
                              left: 4,
                              top: 26,
                              width: r.width - 8,
                              height: r.height - 32,
                              child: CustomPaint(painter: _SparklinePainter())),
                      ])));
  }

  Widget _action() => Positioned(
      left: _actionButton.left,
      top: _actionButton.top,
      width: _actionButton.width,
      height: _actionButton.height,
      child: Stack(children: [
        _button('Map', Icons.map_outlined, 0, 0, 112),
        _button('Ride', Icons.directions_bike, 120, 0, 112,
            bg: AppTheme.cyan, fg: AppTheme.bg),
      ]));

  Widget _dots() => Positioned(
      left: 0,
      top: 302,
      width: 240,
      height: 18,
      child: Stack(children: [
        _text('GPS 12', 4, 0, 76, color: AppTheme.green),
        _text('REC', 80, 0, 70, color: AppTheme.green),
        _text('82%', 150, 0, 64, color: AppTheme.label),
        _text('${pageIndex + 1}/$pageCount', 216, 5, 24,
            size: 8, color: AppTheme.label),
      ]));
}

class _SparklinePainter extends CustomPainter {
  static const List<double> _samples = [
    0.35,
    0.38,
    0.34,
    0.42,
    0.48,
    0.45,
    0.52,
    0.61,
    0.58,
    0.66,
    0.72,
    0.69,
    0.75,
    0.82,
    0.79,
    0.71,
    0.64,
    0.58,
    0.62,
    0.55,
    0.49,
    0.53,
    0.6,
    0.68,
    0.74,
    0.8,
    0.86,
    0.83,
    0.9,
    0.95,
  ];

  @override
  void paint(Canvas canvas, Size size) {
    if (size.height <= 0 || size.width <= 0) return;

    final paint = Paint()
      ..color = AppTheme.green
      ..strokeWidth = 1
      ..style = PaintingStyle.stroke;

    final path = Path();
    for (var i = 0; i < _samples.length; i++) {
      final x = (i / (_samples.length - 1)) * size.width;
      final y = size.height - (_samples[i] * (size.height - 4)) - 2;
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    canvas.drawPath(path, paint);
  }

  @override
  bool shouldRepaint(covariant _SparklinePainter oldDelegate) => false;
}
