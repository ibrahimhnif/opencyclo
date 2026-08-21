import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:opencyclo_app/main.dart';

void main() {
  testWidgets('OpenCyclo App smoke test', (WidgetTester tester) async {
    await tester.pumpWidget(
      const ProviderScope(
        child: OpenCycloApp(),
      ),
    );

    expect(find.text('OPENCYCLO DEVICE'), findsOneWidget);
  });
}
