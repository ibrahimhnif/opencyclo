import 'dart:io';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/models/layout_config_model.dart';
import 'package:opencyclo_app/ui/widgets/device_preview.dart';

void main() {
  test('preview slot rectangles exactly match current C++ templates', () {
    final cpp = File('../src/ui/engine/template_engine.cpp').readAsStringSync();
    final dart = File('lib/ui/widgets/device_preview.dart').readAsStringSync();
    final native = RegExp(r'\{\{(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\},\s*SIZE_')
        .allMatches(cpp)
        .map((m) => [for (var i = 1; i <= 4; i++) int.parse(m[i]!)])
        .toList();
    final preview = RegExp(r'Rect.fromLTWH\((\d+),\s*(\d+),\s*(\d+),\s*(\d+)\)')
        .allMatches(dart)
        .map((m) => [for (var i = 1; i <= 4; i++) int.parse(m[i]!)])
        .take(native.length)
        .toList();
    expect(native.length, 22);
    expect(preview, native);
  });
  test('camera page survives fetch/edit/serialize and unknown ids fail closed',
      () {
    final raw = jsonEncode({
      'page_count': 1,
      'pages': [
        {
          'title': 'camera',
          'template': 4,
          'widgets': [16]
        }
      ]
    });
    final config = UiConfigModel.fromJsonString(raw);
    expect(config.pages.single.widgets.single, WidgetType.cameraRemote);
    expect(jsonDecode(config.toJsonString())['pages'][0]['widgets'], [16]);
    expect(() => WidgetType.fromId(17), throwsFormatException);
  });
  testWidgets('all management pages render and camera Options is preview-only',
      (tester) async {
    for (final page in UiConfigModel.defaultConfig().pages) {
      await tester.pumpWidget(MaterialApp(
          home: Scaffold(
              body: Center(
                  child: SizedBox(
                      width: 240,
                      child: DevicePreview(
                          page: page,
                          title: page.title,
                          pageIndex: 0,
                          pageCount: 5))))));
      await tester.pump();
      expect(tester.takeException(), isNull);
      expect(find.text('GPS 12'), findsOneWidget);
      expect(find.text('pause ride'), findsNothing);
      if (page.widgets.contains(WidgetType.cameraRemote)) {
        expect(find.text('Shutter'), findsOneWidget);
        expect(find.text('Mode'), findsNothing);
        await tester.tap(find.text('Options'));
        await tester.pump();
        for (final label in ['Mode', 'Screen', 'Wake', 'Off', 'Back']) {
          expect(find.text(label), findsOneWidget);
        }
        await tester.tap(find.text('Back'));
        await tester.pump();
        expect(find.text('Shutter'), findsOneWidget);
        expect(page.widgets, [WidgetType.cameraRemote]);
      }
    }
  });
}
