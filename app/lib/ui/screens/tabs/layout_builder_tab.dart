import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../../../core/models/layout_config_model.dart';
import '../../../state/layout_provider.dart';
import '../../theme/app_theme.dart';
import '../../widgets/device_ui.dart';
import '../../widgets/device_preview.dart';
import '../../widgets/grid_slot_card.dart';

/// The layout builder, fronted by a true-to-scale render of the page being
/// edited, so you can see what the panel will actually show before syncing.
/// Every choice offered here is one the device will accept — see GridSlotCard.
class LayoutBuilderTab extends ConsumerStatefulWidget {
  const LayoutBuilderTab({super.key});

  @override
  ConsumerState<LayoutBuilderTab> createState() => _LayoutBuilderTabState();
}

class _LayoutBuilderTabState extends ConsumerState<LayoutBuilderTab> {
  int _selectedPageIndex = 0;

  /// Owned by the state, not rebuilt per frame. The previous implementation
  /// constructed a TextEditingController inside build(), which drops the
  /// cursor on every keystroke; the redesign would have made that very
  /// visible, so it is fixed here.
  final TextEditingController _titleController = TextEditingController();
  String _titleSyncedFrom = '';

  @override
  void dispose() {
    _titleController.dispose();
    super.dispose();
  }

  /// Pull the title in from state only when it changed underneath us (a page
  /// switch, or a fetch from the device) — never while the user is typing.
  void _syncTitleController(String title) {
    if (_titleSyncedFrom != title && _titleController.text != title) {
      _titleController.text = title;
      _titleSyncedFrom = title;
    }
  }

  @override
  Widget build(BuildContext context) {
    final layoutState = ref.watch(layoutProvider);
    final layoutNotifier = ref.read(layoutProvider.notifier);
    final config = layoutState.config;

    if (_selectedPageIndex >= config.pages.length) {
      _selectedPageIndex = 0;
    }

    final currentPage =
        config.pages.isNotEmpty ? config.pages[_selectedPageIndex] : null;

    if (currentPage != null) {
      _syncTitleController(currentPage.title);
    }

    return DeviceScreen(
      statusBar: DeviceStatusBar(
        title: 'builder',
        linkText: 'page ${_selectedPageIndex + 1}/${config.pages.length}',
        linkColor: AppTheme.cyan,
        action: GestureDetector(
          onTap: () => layoutNotifier.fetchFromDevice(),
          behavior: HitTestBehavior.opaque,
          child: const Padding(
            padding: EdgeInsets.only(left: 10),
            child: Icon(Icons.download, size: 17, color: AppTheme.cyan),
          ),
        ),
      ),
      bottomAction: DeviceButton(
        text: layoutState.isSyncing ? 'syncing...' : 'sync to opencyclo',
        color: layoutState.isSyncing ? AppTheme.amber : AppTheme.green,
        icon: Icons.sync,
        busy: layoutState.isSyncing,
        onPressed: layoutState.isSyncing
            ? null
            : () async {
                final success = await layoutNotifier.syncToDevice();
                if (!context.mounted) return;
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(
                    content: Text(
                      success
                          ? 'layout applied to opencyclo'
                          : 'sync failed. check the ble link.',
                      style: AppTheme.statusStyle(
                        success ? AppTheme.green : AppTheme.red,
                      ),
                    ),
                  ),
                );
              },
      ),
      body: currentPage == null
          ? const DeviceEmptyState(text: 'no pages configured.')
          : ListView(
              padding: const EdgeInsets.fromLTRB(
                AppTheme.gutter,
                8,
                AppTheme.gutter,
                24,
              ),
              children: [
                // The panel, exactly as the firmware will draw it.
                Center(
                  child: ConstrainedBox(
                    constraints: const BoxConstraints(maxWidth: 240),
                    child: Container(
                      padding: const EdgeInsets.all(6),
                      decoration: BoxDecoration(
                        border: Border.all(color: AppTheme.hairline),
                        borderRadius: BorderRadius.circular(10),
                      ),
                      child: ClipRRect(
                        borderRadius: BorderRadius.circular(4),
                        child: DevicePreview(
                          page: currentPage,
                          title: currentPage.title,
                          pageIndex: _selectedPageIndex,
                          pageCount: config.pages.length,
                        ),
                      ),
                    ),
                  ),
                ),

                // Page switcher. The preview already draws the device's own dot
                // indicator inside the bezel, so repeating dots here would read
                // as a duplicate — page titles are more useful anyway.
                const SizedBox(height: 12),
                _PageStrip(
                  titles: [for (final p in config.pages) p.title],
                  index: _selectedPageIndex,
                  onSelected: (i) => setState(() {
                    _selectedPageIndex = i;
                    _titleSyncedFrom = '';
                  }),
                ),

                const SizedBox(height: 8),
                const DeviceSectionLabel(text: 'page title'),
                TextField(
                  controller: _titleController,
                  style: AppTheme.valueStyle(AppTheme.text)
                      .copyWith(fontSize: 14),
                  cursorColor: AppTheme.cyan,
                  decoration: const InputDecoration(
                    hintText: 'ride',
                    isDense: true,
                  ),
                  onChanged: (newTitle) {
                    _titleSyncedFrom = newTitle;
                    layoutNotifier.updatePageTitle(_selectedPageIndex, newTitle);
                  },
                ),

                const SizedBox(height: 12),
                _TemplateSelector(
                  value: currentPage.template,
                  onChanged: (t) =>
                      layoutNotifier.updatePageTemplate(_selectedPageIndex, t),
                ),

                const SizedBox(height: 12),
                DeviceSectionLabel(
                  text: 'slots',
                  trailing: Text(
                    '${currentPage.widgets.length} / ${currentPage.template.maxSlots}',
                    style: AppTheme.statusStyle(AppTheme.cyan),
                  ),
                ),

                ...List.generate(currentPage.widgets.length, (slotIdx) {
                  return GridSlotCard(
                    slotIndex: slotIdx,
                    slotSize: currentPage.template.sizeOfSlot(slotIdx),
                    currentWidget: currentPage.widgets[slotIdx],
                    isLast: slotIdx == currentPage.widgets.length - 1,
                    onWidgetChanged: (newWidget) {
                      layoutNotifier.updateSlotWidget(
                        _selectedPageIndex,
                        slotIdx,
                        newWidget,
                      );
                    },
                  );
                }),

                if (layoutState.statusMessage.isNotEmpty) ...[
                  const SizedBox(height: 16),
                  Text(
                    DeviceText.normalise(layoutState.statusMessage),
                    style: AppTheme.labelStyle,
                  ),
                ],
              ],
            ),
    );
  }
}

/// Horizontal page switcher showing each page's own title, active one in cyan.
/// Reads like the device's muted-label / cyan-selection vocabulary without
/// repeating the dot indicator the preview already renders.
class _PageStrip extends StatelessWidget {
  final List<String> titles;
  final int index;
  final ValueChanged<int> onSelected;

  const _PageStrip({
    required this.titles,
    required this.index,
    required this.onSelected,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 30,
      child: ListView.separated(
        scrollDirection: Axis.horizontal,
        itemCount: titles.length,
        separatorBuilder: (_, __) => const SizedBox(width: 20),
        itemBuilder: (context, i) {
          final selected = i == index;
          return GestureDetector(
            behavior: HitTestBehavior.opaque,
            onTap: () => onSelected(i),
            child: Center(
              child: Text(
                DeviceText.normalise(
                  titles[i].isEmpty ? 'page ${i + 1}' : titles[i],
                ),
                style: selected
                    ? AppTheme.statusStyle(AppTheme.cyan).copyWith(fontSize: 13)
                    : AppTheme.labelStyle.copyWith(fontSize: 13),
              ),
            ),
          );
        },
      ),
    );
  }
}

/// Template picker rendered as a settings row rather than a boxed dropdown —
/// the device shows a label on the left and its current value on the right.
class _TemplateSelector extends StatelessWidget {
  final LayoutTemplate value;
  final ValueChanged<LayoutTemplate> onChanged;

  const _TemplateSelector({required this.value, required this.onChanged});

  @override
  Widget build(BuildContext context) {
    return PopupMenuButton<LayoutTemplate>(
      initialValue: value,
      onSelected: onChanged,
      offset: const Offset(0, 8),
      itemBuilder: (context) => LayoutTemplate.values.map((t) {
        return PopupMenuItem<LayoutTemplate>(
          value: t,
          height: 40,
          child: Row(
            children: [
              Text(
                DeviceText.normalise(t.label),
                style: AppTheme.statusStyle(
                  t == value ? AppTheme.cyan : AppTheme.text,
                ).copyWith(fontSize: 13),
              ),
              const Spacer(),
              Text('${t.maxSlots} slots', style: AppTheme.labelStyle),
            ],
          ),
        );
      }).toList(),
      child: DeviceListRow(
        label: 'layout',
        value: DeviceText.normalise(value.label),
        valueColor: AppTheme.cyan,
        trailing: const Icon(
          Icons.keyboard_arrow_down,
          size: 16,
          color: AppTheme.label,
        ),
      ),
    );
  }
}
