import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../core/models/layout_config_model.dart';
import '../../../state/layout_provider.dart';
import '../../theme/app_theme.dart';
import '../../widgets/grid_slot_card.dart';

class LayoutBuilderTab extends ConsumerStatefulWidget {
  const LayoutBuilderTab({super.key});

  @override
  ConsumerState<LayoutBuilderTab> createState() => _LayoutBuilderTabState();
}

class _LayoutBuilderTabState extends ConsumerState<LayoutBuilderTab> {
  int _selectedPageIndex = 0;

  @override
  Widget build(BuildContext context) {
    final layoutState = ref.watch(layoutProvider);
    final layoutNotifier = ref.read(layoutProvider.notifier);
    final config = layoutState.config;

    if (_selectedPageIndex >= config.pages.length) {
      _selectedPageIndex = 0;
    }

    final currentPage = config.pages.isNotEmpty
        ? config.pages[_selectedPageIndex]
        : null;

    return Scaffold(
      appBar: AppBar(
        title: const Text('PAGE & WIDGET BUILDER'),
        actions: [
          IconButton(
            icon: const Icon(Icons.download, color: AppTheme.cyan),
            tooltip: 'Read from OpenCyclo',
            onPressed: () => layoutNotifier.fetchFromDevice(),
          ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Page Selector Tabs
            SizedBox(
              height: 40,
              child: ListView.builder(
                scrollDirection: Axis.horizontal,
                itemCount: config.pages.length,
                itemBuilder: (context, idx) {
                  final isSelected = idx == _selectedPageIndex;
                  return Padding(
                    padding: const EdgeInsets.only(right: 8),
                    child: ChoiceChip(
                      label: Text('Page ${idx + 1}'),
                      selected: isSelected,
                      selectedColor: AppTheme.cyan,
                      backgroundColor: AppTheme.card,
                      labelStyle: TextStyle(
                        color: isSelected ? Colors.black : Colors.white,
                        fontWeight: FontWeight.bold,
                      ),
                      onSelected: (val) {
                        if (val) setState(() => _selectedPageIndex = idx);
                      },
                    ),
                  );
                },
              ),
            ),
            const SizedBox(height: 16),

            if (currentPage != null) ...[
              // Page Title Editor
              TextField(
                decoration: InputDecoration(
                  labelText: 'Page Header Title',
                  labelStyle: const TextStyle(color: AppTheme.textMuted),
                  filled: true,
                  fillColor: AppTheme.card,
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: const BorderSide(color: AppTheme.cardAccent),
                  ),
                ),
                controller: TextEditingController(text: currentPage.title)
                  ..selection = TextSelection.collapsed(offset: currentPage.title.length),
                onChanged: (newTitle) => layoutNotifier.updatePageTitle(_selectedPageIndex, newTitle),
              ),
              const SizedBox(height: 16),

              // Layout Template Selector Dropdown
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                decoration: BoxDecoration(
                  color: AppTheme.card,
                  borderRadius: BorderRadius.circular(8),
                  border: Border.all(color: AppTheme.cardAccent),
                ),
                child: DropdownButtonHideUnderline(
                  child: DropdownButton<LayoutTemplate>(
                    value: currentPage.template,
                    isExpanded: true,
                    dropdownColor: AppTheme.card,
                    items: LayoutTemplate.values.map((t) {
                      return DropdownMenuItem<LayoutTemplate>(
                        value: t,
                        child: Text(
                          t.label,
                          style: const TextStyle(color: Colors.white, fontSize: 14),
                        ),
                      );
                    }).toList(),
                    onChanged: (newTemplate) {
                      if (newTemplate != null) {
                        layoutNotifier.updatePageTemplate(_selectedPageIndex, newTemplate);
                      }
                    },
                  ),
                ),
              ),
              const SizedBox(height: 16),

              // Slot Configuration List
              Text(
                'ASSIGNED DATA SLOTS (${currentPage.widgets.length} / ${currentPage.template.maxSlots})',
                style: const TextStyle(
                  color: AppTheme.textMuted,
                  fontSize: 12,
                  fontWeight: FontWeight.bold,
                  letterSpacing: 1.0,
                ),
              ),
              const SizedBox(height: 8),

              ListView.builder(
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                itemCount: currentPage.widgets.length,
                itemBuilder: (context, slotIdx) {
                  return GridSlotCard(
                    slotIndex: slotIdx,
                    currentWidget: currentPage.widgets[slotIdx],
                    onWidgetChanged: (newWidget) {
                      layoutNotifier.updateSlotWidget(_selectedPageIndex, slotIdx, newWidget);
                    },
                  );
                },
              ),
              const SizedBox(height: 24),

              // Sync to Device Action Button
              ElevatedButton.icon(
                style: ElevatedButton.styleFrom(
                  backgroundColor: AppTheme.green,
                  foregroundColor: Colors.black,
                  padding: const EdgeInsets.symmetric(vertical: 16),
                  shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                ),
                icon: layoutState.isSyncing
                    ? const SizedBox(
                        width: 18,
                        height: 18,
                        child: CircularProgressIndicator(strokeWidth: 2, color: Colors.black),
                      )
                    : const Icon(Icons.sync),
                label: Text(
                  layoutState.isSyncing ? 'SYNCING TO OPENCYCLO...' : 'SYNC TO OPENCYCLO (BLE)',
                  style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
                ),
                onPressed: layoutState.isSyncing
                    ? null
                    : () async {
                        final success = await layoutNotifier.syncToDevice();
                        if (context.mounted) {
                          ScaffoldMessenger.of(context).showSnackBar(
                            SnackBar(
                              content: Text(
                                success
                                    ? 'Layout successfully applied to OpenCyclo!'
                                    : 'Failed to sync layout. Check BLE connection.',
                              ),
                              backgroundColor: success ? AppTheme.green : AppTheme.red,
                            ),
                          );
                        }
                      },
              ),
              if (layoutState.statusMessage.isNotEmpty) ...[
                const SizedBox(height: 10),
                Text(
                  layoutState.statusMessage,
                  style: const TextStyle(color: AppTheme.textMuted, fontSize: 12),
                  textAlign: TextAlign.center,
                ),
              ],
            ],
          ],
        ),
      ),
    );
  }
}
