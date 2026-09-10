#include <unity.h>
#include "navigation/geo.h"
void projection() {
  nav::Point p{0,0};TEST_ASSERT_DOUBLE_WITHIN(0.01,2097152,nav::x(p));TEST_ASSERT_DOUBLE_WITHIN(0.01,2097152,nav::y(p));
}
void matching() {
  double fraction;
  double d=nav::segmentDistance({1000,5000},{0,0},{0,10000},fraction);
  TEST_ASSERT_DOUBLE_WITHIN(0.001,0.5,fraction);TEST_ASSERT_DOUBLE_WITHIN(0.1,11.12,d);
  nav::segmentDistance({0,-5000},{0,0},{0,10000},fraction);TEST_ASSERT_EQUAL_DOUBLE(0,fraction);
}
void packets() {
  nav::Header h{};memcpy(h.magic,"OCR1",4);h.points=2;
  TEST_ASSERT_TRUE(nav::headerValid(h,76));TEST_ASSERT_FALSE(nav::headerValid(h,75));
  h.points=nav::maxPoints+1;TEST_ASSERT_FALSE(nav::headerValid(h,76));
  TEST_ASSERT_EQUAL_HEX32(0xcbf43926,nav::crc32((const uint8_t*)"123456789",9)^0xffffffff);
  TEST_ASSERT_FALSE(nav::valid({900000000,0}));
}
int main() {UNITY_BEGIN();RUN_TEST(projection);RUN_TEST(matching);RUN_TEST(packets);return UNITY_END();}
