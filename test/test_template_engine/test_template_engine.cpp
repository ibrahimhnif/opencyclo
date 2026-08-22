#include <unity.h>
#include "../../src/ui/engine/template_engine.h"

void test_hero_6_grid_slot0_is_hero_sized() {
    const TemplateSlotDefinition& def = getTemplateDefinition(TEMPLATE_HERO_6_GRID);
    TEST_ASSERT_EQUAL(SIZE_HERO, def.slots[0].size_class);
    TEST_ASSERT_EQUAL(232, def.slots[0].rect.w);
    TEST_ASSERT_EQUAL(94, def.slots[0].rect.h);
}

void test_hero_6_grid_bottom_row_is_small() {
    const TemplateSlotDefinition& def = getTemplateDefinition(TEMPLATE_HERO_6_GRID);
    TEST_ASSERT_EQUAL(SIZE_SMALL, def.slots[3].size_class);
    TEST_ASSERT_EQUAL(SIZE_SMALL, def.slots[4].size_class);
    TEST_ASSERT_EQUAL(SIZE_SMALL, def.slots[5].size_class);
}

void test_2_grid_chart_slot2_is_large() {
    const TemplateSlotDefinition& def = getTemplateDefinition(TEMPLATE_2_GRID_CHART);
    TEST_ASSERT_EQUAL(SIZE_LARGE, def.slots[2].size_class);
    TEST_ASSERT_EQUAL(192, def.slots[2].rect.h);
}

void test_full_container_is_full() {
    const TemplateSlotDefinition& def = getTemplateDefinition(TEMPLATE_FULL_CONTAINER);
    TEST_ASSERT_EQUAL(SIZE_FULL, def.slots[0].size_class);
}

void test_out_of_range_template_falls_back_to_full_container() {
    const TemplateSlotDefinition& def = getTemplateDefinition((LayoutTemplateId)99);
    TEST_ASSERT_EQUAL(TEMPLATE_FULL_CONTAINER, def.id);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_hero_6_grid_slot0_is_hero_sized);
    RUN_TEST(test_hero_6_grid_bottom_row_is_small);
    RUN_TEST(test_2_grid_chart_slot2_is_large);
    RUN_TEST(test_full_container_is_full);
    RUN_TEST(test_out_of_range_template_falls_back_to_full_container);
    return UNITY_END();
}
