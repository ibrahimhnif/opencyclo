import 'dart:convert';

enum WidgetType {
  none(0, "None", "-"),
  speed(1, "Speed (Hero)", "KM/H"),
  avgSpeed(2, "Average Speed", "KM/H"),
  maxSpeed(3, "Max Speed", "KM/H"),
  distance(4, "Trip Distance", "KM"),
  rideTime(5, "Ride Time", "TIME"),
  cadence(6, "Cadence", "RPM"),
  heartRate(7, "Heart Rate", "BPM"),
  power(8, "Power Meter", "W"),
  altitude(9, "Altitude", "M"),
  grade(10, "Grade %", "%"),
  totalAscent(11, "Total Ascent", "M"),
  elevationChart(12, "Elevation Profile Chart", "30s"),
  clock(13, "GPS Clock", "TIME"),
  battery(14, "Battery Status", "%"),
  gpsDiagnostics(15, "GPS Diagnostics", "GPS"),
  nmeaConsole(16, "NMEA Live Console", "LOG"),
  bleManager(17, "BLE Sensors Manager", "SENSORS"),
  settingsList(18, "System Settings List", "SETTINGS");

  final int id;
  final String label;
  final String unit;
  const WidgetType(this.id, this.label, this.unit);

  static WidgetType fromId(int id) {
    return WidgetType.values.firstWhere(
      (w) => w.id == id,
      orElse: () => WidgetType.none,
    );
  }
}

enum LayoutTemplate {
  hero6Grid(0, "Hero 6-Grid (1 Hero + 2 Mid + 3 Bot)", 6),
  fourGrid(1, "4-Grid Symmetric (2x2)", 4),
  twoGridChart(2, "2-Grid + Altitude Chart", 3),
  eightGrid(3, "8-Grid Pro View (2x4)", 8),
  fullContainer(4, "Full Container Card", 1);

  final int id;
  final String label;
  final int maxSlots;
  const LayoutTemplate(this.id, this.label, this.maxSlots);

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

  factory UiConfigModel.defaultConfig() {
    return UiConfigModel(
      pageCount: 5,
      pages: [
        PageConfigModel(
          title: "RIDE TELEMETRY",
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
          title: "CLIMB & ELEVATION",
          template: LayoutTemplate.twoGridChart,
          widgets: [
            WidgetType.altitude,
            WidgetType.grade,
            WidgetType.elevationChart,
          ],
        ),
        PageConfigModel(
          title: "BLE & GPS SENSORS",
          template: LayoutTemplate.fullContainer,
          widgets: [WidgetType.bleManager],
        ),
        PageConfigModel(
          title: "NMEA LIVE CONSOLE",
          template: LayoutTemplate.fullContainer,
          widgets: [WidgetType.nmeaConsole],
        ),
        PageConfigModel(
          title: "SYSTEM PREFERENCES",
          template: LayoutTemplate.fullContainer,
          widgets: [WidgetType.settingsList],
        ),
      ],
    );
  }
}
