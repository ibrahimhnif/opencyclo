import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo/ui/screens/gps_assistance_screen.dart';

void main() {
  testWidgets(
      'small screen shows sync and obscures credential without overflow',
      (tester) async {
    tester.view.physicalSize = const Size(360, 640);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    await tester.pumpWidget(const MaterialApp(home: GpsAssistanceScreen()));
    expect(find.text('sync gps'), findsOneWidget);
    expect(
        tester.widget<TextField>(find.byType(TextField)).obscureText, isTrue);
    expect(tester.takeException(), isNull);
    await tester.tap(find.text('sync gps'));
    await tester.pumpAndSettle();
    expect(find.textContaining('Connect and update firmware'), findsOneWidget);
    expect(tester.takeException(), isNull);
  });
}
