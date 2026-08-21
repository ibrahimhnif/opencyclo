import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class AppTheme {
  static const Color background = Color(0xFF0A0E18);
  static const Color card = Color(0xFF141C2C);
  static const Color cardAccent = Color(0xFF202C44);
  static const Color heroBg = Color(0xFF0E1626);

  static const Color cyan = Color(0xFF00D2FF);
  static const Color green = Color(0xFF2ED573);
  static const Color amber = Color(0xFFFFAB00);
  static const Color red = Color(0xFFFF4757);
  static const Color textMuted = Color(0xFF8C9BB4);

  static ThemeData get darkTheme {
    return ThemeData.dark().copyWith(
      scaffoldBackgroundColor: background,
      primaryColor: cyan,
      cardColor: card,
      appBarTheme: const AppBarTheme(
        backgroundColor: card,
        elevation: 0,
        centerTitle: true,
        titleTextStyle: TextStyle(
          color: Colors.white,
          fontSize: 18,
          fontWeight: FontWeight.bold,
          letterSpacing: 1.0,
        ),
      ),
      bottomNavigationBarTheme: const BottomNavigationBarThemeData(
        backgroundColor: card,
        selectedItemColor: cyan,
        unselectedItemColor: textMuted,
        type: BottomNavigationBarType.fixed,
        elevation: 8,
      ),
      textTheme: GoogleFonts.interTextTheme(
        ThemeData.dark().textTheme,
      ),
      colorScheme: const ColorScheme.dark(
        primary: cyan,
        secondary: green,
        surface: card,
        error: red,
      ),
    );
  }
}
