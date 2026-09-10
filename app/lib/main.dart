import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'ui/theme/app_theme.dart';
import 'ui/screens/home_navigation_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();

  // The device is a black panel edge to edge; the app matches it by painting
  // the system bars black with light icons rather than letting the platform
  // tint them.
  SystemChrome.setSystemUIOverlayStyle(
    const SystemUiOverlayStyle(
      statusBarColor: Colors.transparent,
      statusBarIconBrightness: Brightness.light,
      statusBarBrightness: Brightness.dark,
      systemNavigationBarColor: AppTheme.bg,
      systemNavigationBarIconBrightness: Brightness.light,
    ),
  );

  runApp(
    const ProviderScope(
      child: OpenCycloApp(),
    ),
  );
}

class OpenCycloApp extends StatelessWidget {
  const OpenCycloApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'OpenCyclo Companion',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.darkTheme,
      home: const HomeNavigationScreen(),
    );
  }
}
