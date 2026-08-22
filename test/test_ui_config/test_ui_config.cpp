#include <unity.h>
#include "../../src/ui/engine/ui_config.h"
#include <string.h>

void test_valid_config_passes() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION;
    cfg.active_page_count = 4;
    TEST_ASSERT_TRUE(isUiConfigValid(cfg, sizeof(UiConfig)));
}

void test_wrong_byte_count_fails() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION;
    cfg.active_page_count = 4;
    TEST_ASSERT_FALSE(isUiConfigValid(cfg, sizeof(UiConfig) - 1));
}

void test_stale_schema_version_fails() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION - 1; // simulates an older build's saved data
    cfg.active_page_count = 4;
    TEST_ASSERT_FALSE(isUiConfigValid(cfg, sizeof(UiConfig)));
}

void test_zero_pages_fails() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION;
    cfg.active_page_count = 0;
    TEST_ASSERT_FALSE(isUiConfigValid(cfg, sizeof(UiConfig)));
}

void test_too_many_pages_fails() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION;
    cfg.active_page_count = MAX_PAGES + 1;
    TEST_ASSERT_FALSE(isUiConfigValid(cfg, sizeof(UiConfig)));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_config_passes);
    RUN_TEST(test_wrong_byte_count_fails);
    RUN_TEST(test_stale_schema_version_fails);
    RUN_TEST(test_zero_pages_fails);
    RUN_TEST(test_too_many_pages_fails);
    return UNITY_END();
}
