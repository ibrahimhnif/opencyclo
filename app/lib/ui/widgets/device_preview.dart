import 'package:flutter/material.dart';
import '../../core/models/layout_config_model.dart';
import '../theme/app_theme.dart';

/// A true-to-scale render of what a page will look like on the 240x320 panel.
///
/// Slot rectangles are transcribed verbatim from `s_templates[]` in
/// `src/ui/engine/template_engine.cpp`, so the preview shows the real geometry
/// the firmware will use — not an approximation. If a template's rects change
/// there, change them here.
class DevicePreview extends StatelessWidget {
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

  static const double _panelW = 240;
  static const double _panelH = 320;

  /// Verbatim from template_engine.cpp.
  static const Map<LayoutTemplate, List<Rect>> _slots = {
    LayoutTemplate.hero6Grid: [
      Rect.fromLTWH(4, 28, 232, 94),
      Rect.fromLTWH(4, 126, 114, 60),
      Rect.fromLTWH(122, 126, 114, 60),
      Rect.fromLTWH(4, 190, 74, 54),
      Rect.fromLTWH(83, 190, 74, 54),
      Rect.fromLTWH(162, 190, 74, 54),
    ],
    LayoutTemplate.fourGrid: [
      Rect.fromLTWH(4, 28, 114, 130),
      Rect.fromLTWH(122, 28, 114, 130),
      Rect.fromLTWH(4, 164, 114, 134),
      Rect.fromLTWH(122, 164, 114, 134),
    ],
    LayoutTemplate.twoGridChart: [
      Rect.fromLTWH(4, 28, 114, 74),
      Rect.fromLTWH(122, 28, 114, 74),
      Rect.fromLTWH(4, 106, 232, 192),
    ],
    LayoutTemplate.eightGrid: [
      Rect.fromLTWH(4, 28, 114, 64),
      Rect.fromLTWH(122, 28, 114, 64),
      Rect.fromLTWH(4, 96, 114, 64),
      Rect.fromLTWH(122, 96, 114, 64),
      Rect.fromLTWH(4, 164, 114, 64),
      Rect.fromLTWH(122, 164, 114, 64),
      Rect.fromLTWH(4, 232, 114, 64),
      Rect.fromLTWH(122, 232, 114, 64),
    ],
    LayoutTemplate.fullContainer: [
      Rect.fromLTWH(4, 28, 232, 274),
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
                      isHero: page.template == LayoutTemplate.hero6Grid &&
                          i == 0,
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

  /// renderPage segments 1-3, at their real x positions: 4, 76 and 138.
  Widget _statusHeader() {
    return Positioned(
      left: 0,
      top: 5,
      right: 0,
      height: 18,
      child: Stack(
        children: [
          // Clipped to STATUS_TITLE_W. The device gives the title a 68px band
          // and the gps segment's padded erase wipes anything past it on every
          // frame, so an over-long title really does get cut off — the preview
          // shows that rather than hiding it.
          Positioned(
            left: 4,
            width: 68,
            height: 18,
            child: ClipRect(
              child: OverflowBox(
                alignment: Alignment.centerLeft,
                maxWidth: double.infinity,
                child: Align(
                  alignment: Alignment.centerLeft,
                  child: Text(
                    title.toLowerCase(),
                    style: _font(10, AppTheme.label, FontWeight.w400),
                    maxLines: 1,
                    softWrap: false,
                  ),
                ),
              ),
            ),
          ),
          Positioned(
            left: 76,
            width: 58,
            child: Text(
              'gps 11',
              style: _font(10, AppTheme.green, FontWeight.w400),
            ),
          ),
          Positioned(
            left: 138,
            width: 102,
            child: Text(
              'rec 82%',
              style: _font(10, AppTheme.green, FontWeight.w400),
            ),
          ),
        ],
      ),
    );
  }

  Widget _slot(Rect r, WidgetType w, {required bool isHero}) {
    final (label, value, color) = _widgetFace(w);

    return Positioned(
      left: r.left,
      top: r.top,
      width: r.width,
      height: r.height,
      child: w == WidgetType.none
          ? const SizedBox.shrink()
          : Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // renderTile draws its hairline on the tile's top edge.
                Container(height: 1, width: r.width, color: AppTheme.hairline),
                // Label at +4, value at +24 — the firmware's own offsets.
                const SizedBox(height: 3),
                Padding(
                  padding: const EdgeInsets.only(left: 4),
                  child: Text(
                    label,
                    style: _font(9, AppTheme.label, FontWeight.w400),
                    maxLines: 1,
                    overflow: TextOverflow.clip,
                  ),
                ),
                const SizedBox(height: 4),
                Padding(
                  padding: const EdgeInsets.only(left: 4),
                  child: Text(
                    value,
                    style: _font(
                      isHero ? 34 : 15,
                      color,
                      FontWeight.w700,
                    ),
                    maxLines: 1,
                    overflow: TextOverflow.clip,
                  ),
                ),
                if (isHero) ...[
                  const Spacer(),
                  Padding(
                    padding: const EdgeInsets.only(left: 4, bottom: 2),
                    child: Row(
                      children: [
                        const Spacer(),
                        Padding(
                          padding: const EdgeInsets.only(right: 6),
                          child: Text(
                            'km/h',
                            style: _font(9, AppTheme.label, FontWeight.w400),
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
                if (w == WidgetType.elevationChart)
                  Expanded(child: _sparkline(r.width - 8)),
              ],
            ),
    );
  }

  /// A stand-in for renderWidgetElevationChart's 30-sample polyline.
  Widget _sparkline(double width) {
    return Padding(
      padding: const EdgeInsets.only(left: 4, right: 4, bottom: 4),
      child: CustomPaint(
        size: Size(width, double.infinity),
        painter: _SparklinePainter(),
      ),
    );
  }

  /// The green/amber start-ride button, fillRoundRect radius 8, black text.
  Widget _action() {
    return Positioned(
      left: _actionButton.left,
      top: _actionButton.top,
      width: _actionButton.width,
      height: _actionButton.height,
      child: Container(
        decoration: BoxDecoration(
          color: AppTheme.amber,
          borderRadius: BorderRadius.circular(8),
        ),
        alignment: Alignment.center,
        child: Text(
          'pause ride',
          style: _font(14, AppTheme.bg, FontWeight.w700),
        ),
      ),
    );
  }

  /// The page dots the firmware draws at y = 312, centred on x = 120.
  Widget _dots() {
    if (pageCount <= 1) return const SizedBox.shrink();
    final startX = 120 - (pageCount * 8) / 2;

    return Stack(
      children: List.generate(pageCount, (i) {
        final selected = i == pageIndex;
        final r = selected ? 3.0 : 2.0;
        return Positioned(
          left: startX + (i * 8) - r,
          top: 312 - r,
          width: r * 2,
          height: r * 2,
          child: Container(
            decoration: BoxDecoration(
              color: selected ? AppTheme.cyan : AppTheme.label,
              shape: BoxShape.circle,
            ),
          ),
        );
      }),
    );
  }
}

class _SparklinePainter extends CustomPainter {
  static const List<double> _samples = [
    0.35, 0.38, 0.34, 0.42, 0.48, 0.45, 0.52, 0.61, 0.58, 0.66,
    0.72, 0.69, 0.75, 0.82, 0.79, 0.71, 0.64, 0.58, 0.62, 0.55,
    0.49, 0.53, 0.6, 0.68, 0.74, 0.8, 0.86, 0.83, 0.9, 0.95,
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
