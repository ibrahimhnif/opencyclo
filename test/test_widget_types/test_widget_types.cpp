#include <unity.h>
#include "../../src/ui/engine/widget_types.h"

void test_size_class_count_is_five() {
    TEST_ASSERT_EQUAL(5, SIZE_CLASS_COUNT);
}

void test_widget_type_count_is_sixteen() {
    // WIDGET_NONE + 15 real widgets
    TEST_ASSERT_EQUAL(16, WIDGET_TYPE_COUNT);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_size_class_count_is_five);
    RUN_TEST(test_widget_type_count_is_sixteen);
    return UNITY_END();
}
