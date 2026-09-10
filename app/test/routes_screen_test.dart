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
    await tester.scrollUntilVisible(find.text('start GPX navigation'), 100);
    expect(
        tester
            .widget<OutlinedButton>(
                find.widgetWithText(OutlinedButton, 'start GPX navigation'))
            .onPressed,
        isNull);
    expect(find.text('map coverage not checked'), findsOneWidget);
  });
}
