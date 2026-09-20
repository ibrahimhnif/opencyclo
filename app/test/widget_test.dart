import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:opencyclo/main.dart';

void main() {
  testWidgets('OpenCyclo App smoke test', (WidgetTester tester) async {
    await tester.pumpWidget(
      const ProviderScope(
        child: OpenCycloApp(),
      ),
    );

    // The device draws every string lowercase, and so does the app: the status
    // bar's first segment is the screen name, and the nav bar labels match.
    expect(find.text('device'), findsWidgets);
    expect(find.text('scan'), findsOneWidget);
  });

  testWidgets('device tab offers a screenshot button', (WidgetTester tester) async {
    await tester.pumpWidget(const ProviderScope(child: OpenCycloApp()));
    // Rendered alongside sensor debug / assisted GPS; disabled until connected.
    expect(find.text('screenshot'), findsOneWidget);
    expect(find.text('download screenshot'), findsOneWidget);
  });
}
