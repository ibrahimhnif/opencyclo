import 'package:flutter/material.dart';
import '../../core/models/layout_config_model.dart';
import '../theme/app_theme.dart';

class GridSlotCard extends StatelessWidget {
  final int slotIndex;
  final WidgetType currentWidget;
  final ValueChanged<WidgetType> onWidgetChanged;

  const GridSlotCard({
    super.key,
    required this.slotIndex,
    required this.currentWidget,
    required this.onWidgetChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 4),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppTheme.card,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppTheme.cardAccent),
      ),
      child: Row(
        children: [
          Container(
            width: 32,
            height: 32,
            decoration: BoxDecoration(
              color: AppTheme.heroBg,
              borderRadius: BorderRadius.circular(6),
              border: Border.all(color: AppTheme.cyan.withValues(alpha: 0.5)),
            ),
            alignment: Alignment.center,
            child: Text(
              '${slotIndex + 1}',
              style: const TextStyle(
                color: AppTheme.cyan,
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: DropdownButtonHideUnderline(
              child: DropdownButton<WidgetType>(
                value: currentWidget,
                isExpanded: true,
                dropdownColor: AppTheme.card,
                items: WidgetType.values.map((w) {
                  return DropdownMenuItem<WidgetType>(
                    value: w,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.spaceBetween,
                      children: [
                        Text(
                          w.label,
                          style: const TextStyle(color: Colors.white, fontSize: 14),
                        ),
                        if (w.unit.isNotEmpty && w.unit != '-')
                          Text(
                            '[${w.unit}]',
                            style: const TextStyle(color: AppTheme.cyan, fontSize: 12),
                          ),
                      ],
                    ),
                  );
                }).toList(),
                onChanged: (newVal) {
                  if (newVal != null) onWidgetChanged(newVal);
                },
              ),
            ),
          ),
        ],
      ),
    );
  }
}
