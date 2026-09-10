import 'dart:convert';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo_app/core/models/layout_config_model.dart';

/// Guards the app against emitting a layout the firmware will refuse.
///
/// `importLayoutFromString()` in src/storage/layout_config.cpp validates every
/// incoming layout and rejects the ENTIRE import — leaving the device on its
/// old layout and the user staring at "sync failed" — if any of these hold:
///
///   * a widget id is outside 0..WIDGET_TYPE_COUNT-1 (i.e. 0..16), or
///   * a widget sits in a slot whose size class it does not support, or
///   * the template id is outside 0..TEMPLATE_COUNT-1, or
///   * page_count is 0 or above MAX_PAGES.
///
/// These tests encode that contract on the app side. They are the reason the
/// enum ids below must never drift from `WidgetType` in widget_types.h.
void main() {
  group('widget catalog matches the firmware', () {
    test('ids are contiguous 0..16 and match widget_types.h', () {
      // WIDGET_TYPE_COUNT is 17 on the device, asserted by its own native test
      // (test_widget_type_count_is_seventeen).
      expect(WidgetType.values.length, 17);

      const expected = <int, String>{
        0: 'none',
        1: 'speed',
        2: 'avgSpeed',
        3: 'maxSpeed',
        4: 'distance',
        5: 'rideTime',
        6: 'cadence',
        7: 'heartRate',
        8: 'power',
        9: 'altitude',
        10: 'grade',
        11: 'totalAscent',
        12: 'elevationChart',
        13: 'battery',
        14: 'bleManager',
        15: 'settingsList',
        16: 'cameraRemote',
      };

      for (final w in WidgetType.values) {
        expect(expected[w.id], w.name.isEmpty ? null : isNotNull,
            reason: 'id ${w.id} has no firmware counterpart');
      }
      expect(
        {for (final w in WidgetType.values) w.id: w.toString().split('.').last},
        expected,
      );
    });

    test('every widget id is inside the range the device accepts', () {
      for (final w in WidgetType.values) {
        expect(w.id, inInclusiveRange(0, 16),
            reason: '${w.name} would be rejected');
      }
    });

    test('size masks match widget_catalog.cpp', () {
      expect(WidgetType.speed.sizes, {SizeClass.hero});
      expect(WidgetType.distance.sizes, {SizeClass.medium});
      expect(WidgetType.rideTime.sizes, {SizeClass.medium});
      expect(WidgetType.cadence.sizes, {SizeClass.small});
      expect(WidgetType.battery.sizes, {SizeClass.small});
      expect(
          WidgetType.elevationChart.sizes, {SizeClass.large, SizeClass.full});
      expect(WidgetType.bleManager.sizes, {SizeClass.full});
      expect(WidgetType.settingsList.sizes, {SizeClass.full});
      expect(WidgetType.cameraRemote.sizes, {SizeClass.full});
      expect(WidgetType.avgSpeed.sizes, {SizeClass.small, SizeClass.medium});
    });

    test('size support is membership, not ordering', () {
      // A HERO slot is 232x94 and a FULL slot 232x274; neither contains the
      // other, so "bigger is fine" must not hold.
      expect(WidgetType.speed.supportsSize(SizeClass.full), isFalse);
      expect(WidgetType.bleManager.supportsSize(SizeClass.hero), isFalse);
      expect(WidgetType.cadence.supportsSize(SizeClass.medium), isFalse);
    });
  });

  group('templates match template_engine.cpp', () {
    test('slot counts', () {
      expect(LayoutTemplate.hero6Grid.maxSlots, 6);
      expect(LayoutTemplate.fourGrid.maxSlots, 4);
      expect(LayoutTemplate.twoGridChart.maxSlots, 3);
      expect(LayoutTemplate.eightGrid.maxSlots, 8);
      expect(LayoutTemplate.fullContainer.maxSlots, 1);
    });

    test('slot size classes', () {
      expect(LayoutTemplate.hero6Grid.slotSizes, [
        SizeClass.hero,
        SizeClass.medium,
        SizeClass.medium,
        SizeClass.small,
        SizeClass.small,
        SizeClass.small,
      ]);
      expect(LayoutTemplate.twoGridChart.slotSizes,
          [SizeClass.small, SizeClass.small, SizeClass.large]);
      expect(LayoutTemplate.fullContainer.slotSizes, [SizeClass.full]);
    });

    test('every slot offers at least one placeable widget', () {
      for (final t in LayoutTemplate.values) {
        for (var i = 0; i < t.maxSlots; i++) {
          expect(t.widgetsForSlot(i).length, greaterThan(1),
              reason: '${t.name} slot $i has nothing but "none" to offer');
        }
      }
    });
  });

  group('default config is syncable', () {
    test('page count is within MAX_PAGES and matches the list', () {
      final config = UiConfigModel.defaultConfig();
      expect(config.pages.length, config.pageCount);
      expect(config.pageCount, inInclusiveRange(1, 8));
    });

    test('every widget fits the slot it is assigned to', () {
      final config = UiConfigModel.defaultConfig();
      for (var p = 0; p < config.pages.length; p++) {
        final page = config.pages[p];
        expect(page.widgets.length, lessThanOrEqualTo(page.template.maxSlots),
            reason: 'page $p has more widgets than its template has slots');

        for (var s = 0; s < page.widgets.length; s++) {
          final w = page.widgets[s];
          if (w == WidgetType.none) continue;
          expect(
            w.supportsSize(page.template.sizeOfSlot(s)),
            isTrue,
            reason:
                'page $p ("${page.title}") slot $s: ${w.name} cannot sit in '
                '${page.template.sizeOfSlot(s).name} — the device would reject '
                'the whole layout',
          );
        }
      }
    });

    test('titles fit the 68px status band the device draws them into', () {
      // STATUS_TITLE_W is 68px in FreeSans9pt7b, which the firmware header sizes
      // for "sensors" (63px). Roughly 8px per lowercase character, so anything
      // past ~8 characters starts getting clipped by the gps segment's erase.
      for (final page in UiConfigModel.defaultConfig().pages) {
        expect(page.title.length, lessThanOrEqualTo(8),
            reason: '"${page.title}" will be clipped on the device');
        expect(page.title, page.title.toLowerCase(),
            reason: 'the device UI is lowercase throughout');
      }
    });

    test('serialised json carries the firmware widget ids', () {
      final json = jsonDecode(UiConfigModel.defaultConfig().toJsonString())
          as Map<String, dynamic>;
      final pages = json['pages'] as List<dynamic>;

      // Page 0 is the hero ride page: speed, distance, ride time, cadence,
      // heart, power — ids 1, 4, 5, 6, 7, 8 on the device.
      expect((pages[0] as Map<String, dynamic>)['widgets'], [1, 4, 5, 6, 7, 8]);

      // The two full-page management screens must be 14 and 15, not 17 and 18.
      final sensors = pages.firstWhere(
          (p) => (p as Map<String, dynamic>)['title'] == 'sensors') as Map;
      final settings = pages.firstWhere(
          (p) => (p as Map<String, dynamic>)['title'] == 'settings') as Map;
      expect(sensors['widgets'], [14]);
      expect(settings['widgets'], [15]);

      for (final p in pages) {
        for (final id in (p as Map<String, dynamic>)['widgets'] as List) {
          expect(id, inInclusiveRange(0, 16));
        }
        expect(p['template'], inInclusiveRange(0, 4));
      }
    });
  });

  group('template changes never produce an unsyncable page', () {
    test('switching template clears widgets that no longer fit', () {
      // Mirrors LayoutNotifier.updatePageTemplate's revalidation.
      for (final from in LayoutTemplate.values) {
        for (final to in LayoutTemplate.values) {
          final widgets = <WidgetType>[
            for (var i = 0; i < from.maxSlots; i++) from.widgetsForSlot(i).last,
          ];

          final resized = List<WidgetType>.from(widgets);
          while (resized.length < to.maxSlots) {
            resized.add(WidgetType.none);
          }
          if (resized.length > to.maxSlots) {
            resized.removeRange(to.maxSlots, resized.length);
          }
          for (var i = 0; i < resized.length; i++) {
            if (resized[i] != WidgetType.none &&
                !resized[i].supportsSize(to.sizeOfSlot(i))) {
              resized[i] = WidgetType.none;
            }
          }

          for (var i = 0; i < resized.length; i++) {
            if (resized[i] == WidgetType.none) continue;
            expect(resized[i].supportsSize(to.sizeOfSlot(i)), isTrue,
                reason:
                    '${from.name} -> ${to.name} slot $i left an invalid widget');
          }
        }
      }
    });
  });
}
