import 'dart:io';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo/core/models/route_model.dart';
import 'package:opencyclo/ui/screens/tabs/routes_tab.dart';
import 'package:opencyclo/ui/theme/app_theme.dart';

void main() {
  for (final selection in ['cancel', 'oversized', 'unreadable']) {
    testWidgets('Android GPX picker uses any: $selection', (tester) async {
      debugDefaultTargetPlatformOverride = TargetPlatform.android;
      FilePickerIO.registerWith();
      final channel = MethodChannel(
          'miguelruivo.flutter.plugins.filepicker',
          Platform.isLinux || Platform.isWindows || Platform.isMacOS
              ? const JSONMethodCodec()
              : const StandardMethodCodec());
      final messenger =
          TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;
      addTearDown(() {
        debugDefaultTargetPlatformOverride = null;
        messenger.setMockMethodCallHandler(channel, null);
      });
      messenger.setMockMethodCallHandler(channel, (call) async {
        expect(call.method, 'any');
        expect(call.arguments['allowedExtensions'], isNull);
        if (selection == 'cancel') return null;
        return [
          {
            'name': 'test.gpx',
            'path': '/test.gpx',
            'size': selection == 'oversized' ? 9 * 1024 * 1024 : 1,
            'bytes': null,
          }
        ];
      });
      await tester.pumpWidget(MaterialApp(
          theme: AppTheme.darkTheme, home: const Scaffold(body: RoutesTab())));
      await tester.tap(find.text('import GPX'));
      await tester.pumpAndSettle();
      debugDefaultTargetPlatformOverride = null;
      expect(tester.takeException(), isNull);
      expect(find.text('route ready'), findsNothing);
      if (selection == 'cancel') {
        expect(find.text('import a GPX to preview and sync'), findsOneWidget);
      } else {
        expect(find.textContaining('FormatException'), findsOneWidget);
      }
    });
  }
  testWidgets('route preview fits narrow screens and navigation requires sync',
      (tester) async {
    tester.view.physicalSize = const Size(390, 844);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final route = RouteModel.fromGpx(
        '<gpx><rte><rtept lat="-6.2" lon="106.8"/><rtept lat="-6.199" lon="106.8"/><rtept lat="-6.199" lon="106.801"/></rte></gpx>',
        name: 'morning ride');
    await tester.pumpWidget(MaterialApp(
        theme: AppTheme.darkTheme,
        home: Scaffold(body: RoutesTab(initialRoute: route))));
    await tester.pumpAndSettle();
    expect(find.text('morning ride'), findsOneWidget);
    expect(tester.takeException(), isNull);
    expect(find.byIcon(Icons.map_outlined), findsOneWidget);
    expect(find.text('free ride'), findsOneWidget);
    await tester.scrollUntilVisible(find.text('navigate GPX'), 100);
    expect(
        tester
            .widget<OutlinedButton>(
                find.widgetWithText(OutlinedButton, 'navigate GPX'))
            .onPressed,
        isNull);
    expect(find.text('map coverage not checked'), findsOneWidget);
  });
}
