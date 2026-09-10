import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// Design tokens ported 1:1 from the OpenCyclo firmware UI so the companion app
/// and the device read as one product.
///
/// The colours below are byte-identical to the `tft.color565(...)` constants in
/// `src/ui/engine/widget_registry.cpp` / `layout_manager.cpp`. Do not "improve"
/// them here in isolation — if a colour changes, it changes on the device first.
///
/// The device's visual language, which this app reproduces:
///   * Pure black canvas. There are no card fills anywhere on the device.
///   * Structure comes from a single 1px hairline drawn on the TOP edge of a
///     tile (`drawFastHLine(b.x, b.y, b.w, COLOR_HAIRLINE)` in `renderTile`),
///     never from a box, border or shadow.
///   * Every string is lowercase ("ride time", "not paired", "gps: searching...").
///   * Label above in muted grey, value below in white or a status colour.
///   * Units live in the label, not the value — "dist (km)" + "125.75", a
///     deliberate firmware decision so the value never outgrows its tile.
///   * Buttons are the one filled surface: solid colour, black text, small radius.
class AppTheme {
  const AppTheme._();

  // ---------------------------------------------------------------------------
  // Palette — mirrors the firmware colour constants exactly.
  // ---------------------------------------------------------------------------

  /// `COLOR_BG = TFT_BLACK`
  static const Color bg = Color(0xFF000000);

  /// `COLOR_TEXT = TFT_WHITE`
  static const Color text = Color(0xFFFFFFFF);

  /// `COLOR_LABEL = tft.color565(102, 102, 102)`
  static const Color label = Color(0xFF666666);

  /// `COLOR_HAIRLINE = tft.color565(28, 28, 28)`
  static const Color hairline = Color(0xFF1C1C1C);

  /// `COLOR_GREEN = tft.color565(46, 213, 115)` — fix acquired, recording, ok.
  static const Color green = Color(0xFF2ED573);

  /// `COLOR_AMBER = tft.color565(255, 171, 0)` — searching, paused, warning.
  static const Color amber = Color(0xFFFFAB00);

  /// `COLOR_RED = tft.color565(255, 71, 87)` — destructive, error, heart rate.
  static const Color red = Color(0xFFFF4757);

  /// `COLOR_CYAN = tft.color565(0, 210, 255)` — active selection, cadence.
  static const Color cyan = Color(0xFF00D2FF);

  /// The faintest lift off pure black, for text fields and pressed states only.
  /// The device has no equivalent; it exists here because a touch target on a
  /// phone needs an affordance the device gets from its physical bezel.
  static const Color raised = Color(0xFF0B0B0B);

  // ---------------------------------------------------------------------------
  // Geometry — the device's own numbers.
  // ---------------------------------------------------------------------------

  /// `fillRoundRect(..., 8, btnColor)` — the full-width action button.
  static const double radiusAction = 8;

  /// `fillRoundRect(b.x + 6, b.y + 6, b.w - 12, 28, 6, ...)` — the scan button.
  static const double radiusButton = 6;

  /// `fillRoundRect(b.x + b.w - 60, y - 4, 52, 18, 4, COLOR_RED)` — row chips.
  static const double radiusChip = 4;

  /// The device insets tile text by 4px on a 240px panel. Scaled for a phone.
  static const double gutter = 16;

  /// Hairlines are exactly one physical pixel on the device.
  static const double hairlineWidth = 1;

  // ---------------------------------------------------------------------------
  // Type scale.
  //
  // The device uses one family at three sizes: FreeSans 9pt (labels, 18px tall),
  // 12pt (values, 23px) and 24pt (the speed hero). FreeSans is a Helvetica
  // metric clone, so Arimo — metrically compatible with Arial/Helvetica — is the
  // faithful stand-in. One family everywhere, exactly like the device. Bundled
  // as an asset (see pubspec) so it renders identically offline.
  // ---------------------------------------------------------------------------

  static const String fontFamily = 'Arimo';

  static TextStyle _base(double size, Color color, FontWeight weight) {
    return TextStyle(
      fontFamily: fontFamily,
      fontSize: size,
      color: color,
      fontWeight: weight,
      height: 1.2,
    );
  }

  /// FreeSans9pt7b — tile labels, status segments, row labels.
  static TextStyle get labelStyle => _base(11, label, FontWeight.w400)
      .copyWith(letterSpacing: 0.2);

  /// A label that carries state (gps fix, ride state) — same size, colour varies.
  static TextStyle statusStyle(Color color) =>
      _base(11, color, FontWeight.w700).copyWith(letterSpacing: 0.2);

  /// FreeSans12pt7b — tile values, settings values.
  static TextStyle valueStyle(Color color) => _base(22, color, FontWeight.w700);

  /// FreeSans24pt7b — the speed hero, and nothing else.
  static TextStyle get heroStyle =>
      _base(64, text, FontWeight.w700).copyWith(letterSpacing: -1.5);

  /// Body copy. The device has none of this; the app needs it for explanations.
  static TextStyle get bodyStyle => _base(13, label, FontWeight.w400);

  /// Monospaced-feeling detail line (mac addresses, rssi, byte counts).
  static TextStyle get detailStyle =>
      _base(11, label, FontWeight.w400).copyWith(letterSpacing: 0.4);

  // ---------------------------------------------------------------------------
  // ThemeData
  // ---------------------------------------------------------------------------

  static ThemeData get darkTheme {
    final base = ThemeData.dark(useMaterial3: true);

    return base.copyWith(
      scaffoldBackgroundColor: bg,
      canvasColor: bg,
      primaryColor: cyan,
      splashFactory: NoSplash.splashFactory,
      highlightColor: Colors.transparent,
      dividerTheme: const DividerThemeData(
        color: hairline,
        thickness: hairlineWidth,
        space: hairlineWidth,
      ),
      appBarTheme: AppBarTheme(
        backgroundColor: bg,
        surfaceTintColor: Colors.transparent,
        elevation: 0,
        scrolledUnderElevation: 0,
        centerTitle: false,
        systemOverlayStyle: SystemUiOverlayStyle.light,
        titleTextStyle: _base(15, text, FontWeight.w700),
      ),
      bottomNavigationBarTheme: BottomNavigationBarThemeData(
        backgroundColor: bg,
        selectedItemColor: cyan,
        unselectedItemColor: label,
        type: BottomNavigationBarType.fixed,
        elevation: 0,
        showUnselectedLabels: true,
        selectedLabelStyle: _base(10, cyan, FontWeight.w700),
        unselectedLabelStyle: _base(10, label, FontWeight.w400),
      ),
      textTheme: base.textTheme.apply(
        fontFamily: fontFamily,
        bodyColor: text,
        displayColor: text,
      ),
      colorScheme: const ColorScheme.dark(
        primary: cyan,
        onPrimary: bg,
        secondary: green,
        onSecondary: bg,
        surface: bg,
        onSurface: text,
        error: red,
        onError: bg,
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: raised,
        contentPadding:
            const EdgeInsets.symmetric(horizontal: 12, vertical: 14),
        labelStyle: labelStyle,
        floatingLabelStyle: statusStyle(cyan),
        hintStyle: labelStyle,
        border: _fieldBorder(hairline),
        enabledBorder: _fieldBorder(hairline),
        focusedBorder: _fieldBorder(cyan),
      ),
      snackBarTheme: SnackBarThemeData(
        backgroundColor: raised,
        contentTextStyle: _base(13, text, FontWeight.w400),
        behavior: SnackBarBehavior.floating,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(radiusButton),
          side: const BorderSide(color: hairline),
        ),
      ),
      progressIndicatorTheme: const ProgressIndicatorThemeData(
        color: cyan,
        linearTrackColor: hairline,
        circularTrackColor: hairline,
      ),
      popupMenuTheme: PopupMenuThemeData(
        color: raised,
        surfaceTintColor: Colors.transparent,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(radiusButton),
          side: const BorderSide(color: hairline),
        ),
      ),
    );
  }

  static OutlineInputBorder _fieldBorder(Color color) {
    return OutlineInputBorder(
      borderRadius: BorderRadius.circular(radiusButton),
      borderSide: BorderSide(color: color, width: hairlineWidth),
    );
  }

  // ---------------------------------------------------------------------------
  // Semantic helpers — the same mappings the firmware makes.
  // ---------------------------------------------------------------------------

  /// `renderPage` segment 3: green when recording, amber when paused, muted idle.
  static Color rideStateColor(int rideState) {
    if (rideState == 1) return green;
    if (rideState == 2) return amber;
    return label;
  }

  /// `renderPage` segment 3 text: "rec" / "pause" / "stop".
  static String rideStateText(int rideState) {
    if (rideState == 1) return 'rec';
    if (rideState == 2) return 'pause';
    return 'stop';
  }

  /// `renderWidgetBattery`: amber below 20%, green otherwise.
  static Color batteryColor(int pct) => pct < 20 ? amber : green;

  /// `renderWidgetGrade`: amber climbing, cyan descending, green flat.
  static Color gradeColor(double pct) {
    if (pct > 3.0) return amber;
    if (pct < -2.0) return cyan;
    return green;
  }
}
