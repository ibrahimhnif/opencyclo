#include <unity.h>
#include "../../src/ui/engine/widget_catalog.h"

void test_speed_supports_only_hero() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_SPEED, SIZE_HERO));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_SPEED, SIZE_SMALL));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_SPEED, SIZE_MEDIUM));
}

void test_cadence_supports_only_small() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_CADENCE, SIZE_SMALL));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_CADENCE, SIZE_HERO));
}

void test_avg_speed_supports_small_and_medium() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_AVG_SPEED, SIZE_SMALL));
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_AVG_SPEED, SIZE_MEDIUM));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_AVG_SPEED, SIZE_HERO));
}

void test_elevation_chart_supports_large_and_full() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_ELEVATION_CHART, SIZE_LARGE));
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_ELEVATION_CHART, SIZE_FULL));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_ELEVATION_CHART, SIZE_SMALL));
}

void test_ble_manager_supports_only_full() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_BLE_MANAGER, SIZE_FULL));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_BLE_MANAGER, SIZE_HERO));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_BLE_MANAGER, SIZE_SMALL));
}

void test_widget_none_supports_nothing() {
    for (uint8_t s = 0; s < SIZE_CLASS_COUNT; s++) {
        TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_NONE, (SizeClass)s));
    }
}

void test_getWidgetMeta_returns_matching_type() {
    TEST_ASSERT_EQUAL(WIDGET_SPEED, getWidgetMeta(WIDGET_SPEED).type);
    TEST_ASSERT_EQUAL_STRING("SPEED", getWidgetMeta(WIDGET_SPEED).label);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_speed_supports_only_hero);
    RUN_TEST(test_cadence_supports_only_small);
    RUN_TEST(test_avg_speed_supports_small_and_medium);
    RUN_TEST(test_elevation_chart_supports_large_and_full);
    RUN_TEST(test_ble_manager_supports_only_full);
    RUN_TEST(test_widget_none_supports_nothing);
    RUN_TEST(test_getWidgetMeta_returns_matching_type);
    return UNITY_END();
}
