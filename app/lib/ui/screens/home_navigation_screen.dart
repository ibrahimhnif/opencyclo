import 'package:flutter/material.dart';
import 'tabs/device_tab.dart';
import 'tabs/layout_builder_tab.dart';
import 'tabs/live_dashboard_tab.dart';
import 'tabs/ota_update_tab.dart';

class HomeNavigationScreen extends StatefulWidget {
  const HomeNavigationScreen({super.key});

  @override
  State<HomeNavigationScreen> createState() => _HomeNavigationScreenState();
}

class _HomeNavigationScreenState extends State<HomeNavigationScreen> {
  int _currentIndex = 0;

  final List<Widget> _tabs = const [
    DeviceTab(),
    LayoutBuilderTab(),
    LiveDashboardTab(),
    OtaUpdateTab(),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: IndexedStack(
        index: _currentIndex,
        children: _tabs,
      ),
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _currentIndex,
        onTap: (index) => setState(() => _currentIndex = index),
        items: const [
          BottomNavigationBarItem(
            icon: Icon(Icons.bluetooth),
            label: 'Device',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.dashboard_customize),
            label: 'Builder',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.speed),
            label: 'Live',
          ),
          BottomNavigationBarItem(
            icon: Icon(Icons.system_update_alt),
            label: 'OTA',
          ),
        ],
      ),
    );
  }
}
