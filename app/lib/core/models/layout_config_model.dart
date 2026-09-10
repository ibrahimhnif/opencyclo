import 'dart:convert';

/// Slot size classes, mirroring `SizeClass` in `src/ui/engine/widget_types.h`.
///
/// These are NOT ordered by area and a bigger slot is not automatically valid:
/// LARGE (232x192), HERO (232x94) and FULL (232x274) are different footprints.
/// Placement is exact bitmask membership, exactly as `widgetSupportsSize()`
/// enforces on the device.
enum SizeClass {
  small(0, '74x54'),
  medium(1, '114x60'),
  large(2, '232x192'),
  hero(3, '232x94'),
  full(4, '232x274');

  final int id;
  final String dimensions;
  const SizeClass(this.id, this.dimensions);
}

/// A 1:1 port of `s_catalog[]` in `src/ui/engine/widget_catalog.cpp`.
///
/// The ids here are the wire format: `toJsonString()` writes them straight into
/// the layout JSON, and the firmware's `importLayoutFromString()` range-checks
/// each one against `WIDGET_TYPE_COUNT` and REJECTS THE WHOLE IMPORT on a
/// mismatch. They must stay identical to `WidgetType` in `widget_types.h`.
///
/// `label` is the string the device actually draws for the widget, so the
/// builder names a widget the same way the panel will. `sizes` is the firmware's
/// `supported_sizes_mask`, which the device also validates on import — a widget
/// in a slot it does not support rejects the entire layout, not just that slot.
///
/// There is deliberately no entry here without a firmware counterpart. The
/// device can only render ids 0..15; anything else fails the import outright.
enum WidgetType {
  none(0, 'None', '', '', '', {}),
  speed(1, 'Speed', 'speed', 'km/h', 'mph', {SizeClass.hero}),
  avgSpeed(2, 'Avg Speed', 'avg spd', 'km/h', 'mph',
      {SizeClass.small, SizeClass.medium}),
  maxSpeed(3, 'Max Speed', 'max spd', 'km/h', 'mph',
      {SizeClass.small, SizeClass.medium}),
  distance(4, 'Distance', 'dist', 'km', 'mi', {SizeClass.medium}),
  rideTime(5, 'Ride Time', 'ride time', '', '', {SizeClass.medium}),
  cadence(6, 'Cadence', 'cadence', 'rpm', 'rpm', {SizeClass.small}),
  heartRate(7, 'Heart Rate', 'heart', 'bpm', 'bpm', {SizeClass.small}),
  power(8, 'Power', 'power', 'w', 'w', {SizeClass.small}),
  altitude(9, 'Altitude', 'alt', 'm', 'ft',
      {SizeClass.small, SizeClass.medium}),
  grade(10, 'Grade', 'grade', '%', '%', {SizeClass.small, SizeClass.medium}),
  totalAscent(11, 'Total Ascent', 'asc', 'm', 'ft',
      {SizeClass.small, SizeClass.medium}),
  elevationChart(12, 'Elevation Chart', 'elevation profile', '', '',
      {SizeClass.large, SizeClass.full}),
  battery(13, 'Battery', 'battery', '%', '%', {SizeClass.small}),
  bleManager(14, 'BLE Manager', 'sensors', '', '', {SizeClass.full}),
  settingsList(15, 'Settings List', 'settings', '', '', {SizeClass.full});

  final int id;

  /// Title-case picker name, from the catalog's `name` column.
  final String name;

  /// The literal the device draws, from the catalog's `label` column.
  final String label;

  final String unitMetric;
  final String unitImperial;

  /// The firmware's `supported_sizes_mask`, as a set.
  final Set<SizeClass> sizes;

  const WidgetType(this.id, this.name, this.label, this.unitMetric,
      this.unitImperial, this.sizes);

  /// Mirrors `widgetSupportsSize()`. A widget may only sit in a slot whose size
  /// class it declares — the device rejects the whole layout otherwise.
  bool supportsSize(SizeClass size) => sizes.contains(size);

  String unit(bool imperial) => imperial ? unitImperial : unitMetric;

  static WidgetType fromId(int id) {
    return WidgetType.values.firstWhere(
      (w) => w.id == id,
      orElse: () => WidgetType.none,
    );
  }
}

/// Ports `s_templates[]` from `src/ui/engine/template_engine.cpp`, including
/// each slot's size class so the builder can only offer placements the device
/// will accept.
enum LayoutTemplate {
  hero6Grid(0, 'Hero 6-Grid', [
    SizeClass.hero,
    SizeClass.medium,
    SizeClass.medium,
    SizeClass.small,
    SizeClass.small,
    SizeClass.small,
  ]),
  fourGrid(1, '4-Grid Symmetric', [
    SizeClass.medium,
    SizeClass.medium,
    SizeClass.medium,
    SizeClass.medium,
  ]),
  twoGridChart(2, '2-Grid + Chart', [
    SizeClass.small,
    SizeClass.small,
    SizeClass.large,
  ]),
  eightGrid(3, '8-Grid Pro View', [
    SizeClass.small,
    SizeClass.small,
    SizeClass.small,
    SizeClass.small,
    SizeClass.small,
    SizeClass.small,
    SizeClass.small,
    SizeClass.small,
  ]),
  fullContainer(4, 'Full Container', [SizeClass.full]);

  final int id;

  /// The firmware's own template name, from `s_templates[].name`.
  final String label;

  final List<SizeClass> slotSizes;

  const LayoutTemplate(this.id, this.label, this.slotSizes);

  int get maxSlots => slotSizes.length;

  SizeClass sizeOfSlot(int index) =>
      (index >= 0 && index < slotSizes.length) ? slotSizes[index] : slotSizes.last;

  /// Widgets the device will accept in this slot, always including `none` so a
  /// slot can be emptied.
  List<WidgetType> widgetsForSlot(int index) {
    final size = sizeOfSlot(index);
    return [
      WidgetType.none,
      ...WidgetType.values.where(
        (w) => w != WidgetType.none && w.supportsSize(size),
      ),
    ];
  }

  static LayoutTemplate fromId(int id) {
    return LayoutTemplate.values.firstWhere(
      (t) => t.id == id,
      orElse: () => LayoutTemplate.hero6Grid,
    );
  }
}

class PageConfigModel {
  String title;
  LayoutTemplate template;
  List<WidgetType> widgets;

  PageConfigModel({
    required this.title,
    required this.template,
    required this.widgets,
  });

  Map<String, dynamic> toJson() => {
    'title': title,
    'template': template.id,
    'widgets': widgets.map((w) => w.id).toList(),
  };

  factory PageConfigModel.fromJson(Map<String, dynamic> json) {
    final title = json['title'] as String? ?? 'Page';
    final template = LayoutTemplate.fromId(json['template'] as int? ?? 0);
    final rawWidgets = json['widgets'] as List<dynamic>? ?? [];
    final widgets = rawWidgets.map((e) => WidgetType.fromId(e as int)).toList();

    return PageConfigModel(
      title: title,
      template: template,
      widgets: widgets,
    );
  }
}

class UiConfigModel {
  int pageCount;
  List<PageConfigModel> pages;

  UiConfigModel({
    required this.pageCount,
    required this.pages,
  });

  String toJsonString() {
    return jsonEncode({
      'page_count': pageCount,
      'pages': pages.map((p) => p.toJson()).toList(),
    });
  }

  factory UiConfigModel.fromJsonString(String jsonStr) {
    final Map<String, dynamic> json = jsonDecode(jsonStr);
    final count = json['page_count'] as int? ?? 0;
    final rawPages = json['pages'] as List<dynamic>? ?? [];
    final pages = rawPages.map((p) => PageConfigModel.fromJson(p as Map<String, dynamic>)).toList();

    return UiConfigModel(pageCount: count, pages: pages);
  }

  /// Mirrors the firmware's own defaults in `src/storage/layout_config.cpp`.
  ///
  /// Titles are short and lowercase on purpose: the device draws the title into
  /// a 68px band (`STATUS_TITLE_W` in layout_manager.cpp, sized for "sensors" at
  /// 63px), and the gps segment's padded erase clips anything wider on every
  /// frame. "RIDE TELEMETRY" measures well past that and would render truncated
  /// on the panel.
  factory UiConfigModel.defaultConfig() {
    return UiConfigModel(
      pageCount: 4,
      pages: [
        PageConfigModel(
          title: "ride",
          template: LayoutTemplate.hero6Grid,
          widgets: [
            WidgetType.speed,
            WidgetType.distance,
            WidgetType.rideTime,
            WidgetType.cadence,
            WidgetType.heartRate,
            WidgetType.power,
          ],
        ),
        PageConfigModel(
          title: "climb",
          template: LayoutTemplate.twoGridChart,
          widgets: [
            WidgetType.altitude,
            WidgetType.grade,
            WidgetType.elevationChart,
          ],
        ),
        PageConfigModel(
          title: "sensors",
          template: LayoutTemplate.fullContainer,
          widgets: [WidgetType.bleManager],
        ),
        PageConfigModel(
          title: "settings",
          template: LayoutTemplate.fullContainer,
          widgets: [WidgetType.settingsList],
        ),
      ],
    );
  }
}
