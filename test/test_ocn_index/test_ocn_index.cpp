#include <unity.h>
#include "navigation/ocn_index.h"
static uint8_t table[3*12];
static void entry(uint32_t i,uint32_t y,uint32_t offset,uint32_t count) {
  const uint32_t v[3]={y,offset,count};
  for(uint32_t k=0;k<3;k++)for(uint32_t b=0;b<4;b++)table[i*12+k*4+b]=uint8_t(v[k]>>(8*b));
}
void findsRows() {
  entry(0,10,0,3);entry(1,20,27,5);entry(2,30,77,1);
  uint32_t offset=0xdead,count=0xbeef;
  TEST_ASSERT_TRUE(ocn::findRow(table,3,10,&offset,&count));
  TEST_ASSERT_EQUAL_UINT32(0,offset);TEST_ASSERT_EQUAL_UINT32(3,count);
  TEST_ASSERT_TRUE(ocn::findRow(table,3,20,&offset,&count));
  TEST_ASSERT_EQUAL_UINT32(27,offset);TEST_ASSERT_EQUAL_UINT32(5,count);
  TEST_ASSERT_TRUE(ocn::findRow(table,3,30,&offset,&count));
  TEST_ASSERT_EQUAL_UINT32(77,offset);TEST_ASSERT_EQUAL_UINT32(1,count);
}
void rejectsMissingRows() {
  entry(0,10,0,3);entry(1,20,27,5);entry(2,30,77,1);
  uint32_t offset=0xdead,count=0xbeef;
  TEST_ASSERT_FALSE(ocn::findRow(table,3,9,&offset,&count));   // below the first
  TEST_ASSERT_FALSE(ocn::findRow(table,3,15,&offset,&count));  // in a gap
  TEST_ASSERT_FALSE(ocn::findRow(table,3,31,&offset,&count));  // past the last
  TEST_ASSERT_FALSE(ocn::findRow(table,0,10,&offset,&count));  // empty index
  TEST_ASSERT_EQUAL_UINT32(0xdead,offset);TEST_ASSERT_EQUAL_UINT32(0xbeef,count);
}
void accumulatedOffsets() {
  // Mirrors the Python builder's cross-row offset accumulation: row 8192 holds
  // two 10-byte segments, so row 8193 starts at byte 20 of the segment section.
  entry(0,8192,0,2);entry(1,8193,20,1);
  uint32_t offset=0,count=0;
  TEST_ASSERT_TRUE(ocn::findRow(table,2,8192,&offset,&count));
  TEST_ASSERT_EQUAL_UINT32(0,offset);TEST_ASSERT_EQUAL_UINT32(2,count);
  TEST_ASSERT_TRUE(ocn::findRow(table,2,8193,&offset,&count));
  TEST_ASSERT_EQUAL_UINT32(20,offset);TEST_ASSERT_EQUAL_UINT32(1,count);
  TEST_ASSERT_FALSE(ocn::findRow(table,2,8194,&offset,&count));
}
int main() {UNITY_BEGIN();RUN_TEST(findsRows);RUN_TEST(rejectsMissingRows);RUN_TEST(accumulatedOffsets);return UNITY_END();}
