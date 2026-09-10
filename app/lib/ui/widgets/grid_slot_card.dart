import 'package:flutter/material.dart';
import '../../core/models/layout_config_model.dart';
import '../theme/app_theme.dart';
import 'device_ui.dart';

/// One slot assignment, drawn as a device settings row: muted label on the
/// left, the assigned widget in cyan on the right, hairline divider below.
///
/// The picker only offers widgets the device will accept in this slot. That is
/// not cosmetic filtering — `importLayoutFromString()` rejects the *entire*
/// layout if any widget sits in a size class it does not support, so offering
/// an invalid choice would turn into a whole failed sync with no explanation.
class GridSlotCard extends StatelessWidget {
  final int slotIndex;

  /// The slot's size class, from the page template. Determines what may go here.
  final SizeClass slotSize;

  final WidgetType currentWidget;
  final ValueChanged<WidgetType> onWidgetChanged;
  final bool isLast;

  const GridSlotCard({
    super.key,
    required this.slotIndex,
    required this.slotSize,
    required this.currentWidget,
    required this.onWidgetChanged,
    this.isLast = false,
  });

  @override
  Widget build(BuildContext context) {
    final assigned = currentWidget != WidgetType.none;

    // A config loaded from an older app build (or hand-edited) can hold a
    // widget this slot cannot take. Surface it in amber rather than letting the
    // sync fail silently later.
    final invalid = assigned && !currentWidget.supportsSize(slotSize);

    final options = [
      WidgetType.none,
      ...WidgetType.values.where(
        (w) => w != WidgetType.none && w.supportsSize(slotSize),
      ),
    ];

    return PopupMenuButton<WidgetType>(
      initialValue: options.contains(currentWidget) ? currentWidget : null,
      onSelected: onWidgetChanged,
      offset: const Offset(0, 8),
      constraints: const BoxConstraints(minWidth: 240, maxHeight: 400),
      itemBuilder: (context) => options.map((w) {
        final selected = w == currentWidget;
        return PopupMenuItem<WidgetType>(
          value: w,
          height: 38,
          child: Row(
            children: [
              Text(
                w == WidgetType.none ? 'empty' : w.label,
                style: AppTheme.statusStyle(
                  selected ? AppTheme.cyan : AppTheme.text,
                ).copyWith(fontSize: 13),
              ),
              if (w.unitMetric.isNotEmpty) ...[
                const Spacer(),
                Text(w.unitMetric, style: AppTheme.labelStyle),
              ],
            ],
          ),
        );
      }).toList(),
      child: DeviceListRow(
        label: 'slot ${slotIndex + 1}',
        detail: slotSize.dimensions,
        value: currentWidget == WidgetType.none
            ? 'empty'
            : currentWidget.label + (invalid ? '  ·  wrong size' : ''),
        valueColor: invalid
            ? AppTheme.amber
            : (assigned ? AppTheme.cyan : AppTheme.label),
        showDivider: !isLast,
        trailing: const Icon(
          Icons.keyboard_arrow_down,
          size: 16,
          color: AppTheme.label,
        ),
      ),
    );
  }
}
