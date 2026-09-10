import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/models/route_model.dart';
import 'package:opencyclo_app/ui/screens/tabs/routes_tab.dart';
import 'package:opencyclo_app/ui/theme/app_theme.dart';

void main() {
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
