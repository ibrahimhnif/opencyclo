import 'package:flutter/material.dart';
import '../theme/app_theme.dart';
import '../widgets/device_ui.dart';
import 'tabs/device_tab.dart';
import 'tabs/layout_builder_tab.dart';
import 'tabs/live_dashboard_tab.dart';
import 'tabs/ota_update_tab.dart';
import 'tabs/routes_tab.dart';

class HomeNavigationScreen extends StatefulWidget {
  const HomeNavigationScreen({super.key});

  @override
  State<HomeNavigationScreen> createState() => _HomeNavigationScreenState();
}

class _HomeNavigationScreenState extends State<HomeNavigationScreen> {
  int _currentIndex = 0;

  static const List<Widget> _tabs = [
    DeviceTab(),
    LayoutBuilderTab(),
    LiveDashboardTab(),
    RoutesTab(),
    OtaUpdateTab(),
  ];

  static const List<({IconData icon, String label})> _items = [
    (icon: Icons.bluetooth, label: 'device'),
    (icon: Icons.grid_view, label: 'builder'),
    (icon: Icons.speed, label: 'live'),
    (icon: Icons.route, label: 'routes'),
    (icon: Icons.bolt, label: 'ota'),
  ];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bg,
      body: IndexedStack(index: _currentIndex, children: _tabs),
      bottomNavigationBar: _DeviceNavBar(
        index: _currentIndex,
        items: _items,
        onTap: (i) => setState(() => _currentIndex = i),
      ),
    );
  }
}

/// A nav bar in the device's language: black, separated from the content by a
/// single hairline, cyan for the active item and COLOR_LABEL grey for the rest.
/// No pill, no elevation, no tint — the device has none of those.
class _DeviceNavBar extends StatelessWidget {
  final int index;
  final List<({IconData icon, String label})> items;
  final ValueChanged<int> onTap;

  const _DeviceNavBar({
    required this.index,
    required this.items,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      color: AppTheme.bg,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const DeviceHairline(),
          SafeArea(
            top: false,
            child: SizedBox(
              height: 56,
              child: Row(
                children: List.generate(items.length, (i) {
                  final selected = i == index;
                  final color = selected ? AppTheme.cyan : AppTheme.label;

                  return Expanded(
                    child: GestureDetector(
                      behavior: HitTestBehavior.opaque,
                      onTap: () => onTap(i),
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Icon(items[i].icon, size: 20, color: color),
                          const SizedBox(height: 5),
                          Text(
                            items[i].label,
                            style: AppTheme.statusStyle(color)
                                .copyWith(fontSize: 10),
                          ),
                        ],
                      ),
                    ),
                  );
                }),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
