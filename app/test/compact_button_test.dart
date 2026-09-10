import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/ui/theme/app_theme.dart';
import 'package:opencyclo_app/ui/widgets/device_ui.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  setUpAll(() async {
    // Measure bundled Arimo, not Flutter test's deliberately wide Ahem font.
    final font = FontLoader('Arimo')
      ..addFont(rootBundle.load('assets/fonts/Arimo-Regular.ttf'))
      ..addFont(rootBundle.load('assets/fonts/Arimo-Bold.ttf'));
    await font.load();
  });
  testWidgets('short icon labels fit narrow buttons; tap and busy guards stay',
      (tester) async {
    var taps = 0;
    for (final scale in [1.0, 1.5]) {
      for (final label in [
        'scan',
        'sync layout',
        'update',
        'choose .bin'
      ]) {
        await tester.pumpWidget(MaterialApp(
          theme: AppTheme.darkTheme,
          home: MediaQuery(
            data: MediaQueryData(textScaler: TextScaler.linear(scale)),
            child: Scaffold(
              body: Center(
                child: SizedBox(
                  width: 240,
                  child: DeviceButton(
                    text: label,
                    color: AppTheme.cyan,
                    icon: Icons.sync,
                    onPressed: () => taps++,
                  ),
                ),
              ),
            ),
          ),
        ));
        expect(tester.takeException(), isNull);
        final text = tester.widget<Text>(find.text(label));
        final painter = TextPainter(
          text: TextSpan(text: label, style: text.style),
          textDirection: TextDirection.ltr,
          textScaler: TextScaler.linear(scale),
          maxLines: 1,
        )..layout();
        expect(painter.width, lessThanOrEqualTo(210), reason: '$label at $scale');
        painter.dispose();
        expect(tester.getSize(find.byType(DeviceButton)).height, 52);
        final before = taps;
        await tester.tap(find.byType(DeviceButton));
        expect(taps, before + 1);
      }
    }
    await tester.pumpWidget(MaterialApp(
      home: Scaffold(
        body: DeviceButton(
          text: 'syncing...',
          color: AppTheme.cyan,
          busy: true,
          onPressed: () => taps++,
        ),
      ),
    ));
    final before = taps;
    await tester.tap(find.byType(DeviceButton));
    expect(taps, before);
    expect(find.byType(CircularProgressIndicator), findsOneWidget);
  });
}
