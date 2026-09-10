import 'package:flutter_riverpod/flutter_riverpod.dart';
import '../core/ble/ble_service.dart';
import '../core/models/layout_config_model.dart';

class LayoutState {
  final UiConfigModel config;
  final bool isLoading;
  final bool isSyncing;
  final String statusMessage;

  const LayoutState({
    required this.config,
    this.isLoading = false,
    this.isSyncing = false,
    this.statusMessage = '',
  });

  LayoutState copyWith({
    UiConfigModel? config,
    bool? isLoading,
    bool? isSyncing,
    String? statusMessage,
  }) {
    return LayoutState(
      config: config ?? this.config,
      isLoading: isLoading ?? this.isLoading,
      isSyncing: isSyncing ?? this.isSyncing,
      statusMessage: statusMessage ?? this.statusMessage,
    );
  }
}

class LayoutNotifier extends StateNotifier<LayoutState> {
  LayoutNotifier() : super(LayoutState(config: UiConfigModel.defaultConfig()));

  Future<void> fetchFromDevice() async {
    state = state.copyWith(isLoading: true, statusMessage: 'Reading layout from OpenCyclo...');
    final fetched = await BleService.instance.fetchLayoutConfig();
    if (fetched != null) {
      state = state.copyWith(config: fetched, isLoading: false, statusMessage: 'Layout loaded from device');
    } else {
      state = state.copyWith(isLoading: false, statusMessage: 'Failed to read layout from device');
    }
  }

  Future<bool> syncToDevice() async {
    state = state.copyWith(isSyncing: true, statusMessage: 'Sending layout to OpenCyclo via BLE...');
    final success = await BleService.instance.sendLayoutConfig(state.config);
    state = state.copyWith(
      isSyncing: false,
      statusMessage: success ? 'Layout synced successfully!' : 'Failed to sync layout',
    );
    return success;
  }

  void updatePageTemplate(int pageIndex, LayoutTemplate template) {
    if (pageIndex >= state.config.pages.length) return;

    final updatedPages = List<PageConfigModel>.from(state.config.pages);
    final currentPage = updatedPages[pageIndex];

    // Resize to the new template's slot count, padding with empty slots. Never
    // pad with a concrete widget: `speed` only supports SIZE_HERO, so filling
    // four medium slots with it produced a layout the device rejects outright
    // (importLayoutFromString() fails the WHOLE import on one bad placement).
    // An empty slot is explicitly legal on the device and draws nothing.
    final updatedWidgets = List<WidgetType>.from(currentPage.widgets);
    while (updatedWidgets.length < template.maxSlots) {
      updatedWidgets.add(WidgetType.none);
    }
    if (updatedWidgets.length > template.maxSlots) {
      updatedWidgets.removeRange(template.maxSlots, updatedWidgets.length);
    }

    // Slot size classes differ between templates, so a widget that was valid
    // before may not fit where it now lands. Clear those rather than carrying
    // an unsyncable layout forward.
    for (var i = 0; i < updatedWidgets.length; i++) {
      final w = updatedWidgets[i];
      if (w != WidgetType.none && !w.supportsSize(template.sizeOfSlot(i))) {
        updatedWidgets[i] = WidgetType.none;
      }
    }

    updatedPages[pageIndex] = PageConfigModel(
      title: currentPage.title,
      template: template,
      widgets: updatedWidgets,
    );

    state = state.copyWith(config: UiConfigModel(pageCount: state.config.pageCount, pages: updatedPages));
  }

  void updateSlotWidget(int pageIndex, int slotIndex, WidgetType newWidget) {
    if (pageIndex >= state.config.pages.length) return;

    final updatedPages = List<PageConfigModel>.from(state.config.pages);
    final currentPage = updatedPages[pageIndex];

    final updatedWidgets = List<WidgetType>.from(currentPage.widgets);
    if (slotIndex < updatedWidgets.length) {
      updatedWidgets[slotIndex] = newWidget;
    }

    updatedPages[pageIndex] = PageConfigModel(
      title: currentPage.title,
      template: currentPage.template,
      widgets: updatedWidgets,
    );

    state = state.copyWith(config: UiConfigModel(pageCount: state.config.pageCount, pages: updatedPages));
  }

  void updatePageTitle(int pageIndex, String newTitle) {
    if (pageIndex >= state.config.pages.length) return;

    final updatedPages = List<PageConfigModel>.from(state.config.pages);
    updatedPages[pageIndex].title = newTitle;

    state = state.copyWith(config: UiConfigModel(pageCount: state.config.pageCount, pages: updatedPages));
  }
}

final layoutProvider = StateNotifierProvider<LayoutNotifier, LayoutState>((ref) {
  return LayoutNotifier();
});
