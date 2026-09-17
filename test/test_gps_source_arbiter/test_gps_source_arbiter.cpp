#include <unity.h>
#include "../../src/hardware/gps_source_arbiter.h"

void test_hardware_mode_uses_hardware_when_valid() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_HARDWARE, true, true, 0);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_HARDWARE, s);
}

void test_hardware_mode_falls_back_to_phone_when_hardware_invalid_and_phone_fresh() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_HARDWARE, false, true, 1000);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_PHONE_FALLBACK, s);
}

void test_hardware_mode_ignores_stale_phone_fix() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_HARDWARE, false, true, PHONE_FIX_STALE_MS);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_NONE, s);
}

void test_hardware_mode_no_fix_when_both_unavailable() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_HARDWARE, false, false, 0);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_NONE, s);
}

void test_phone_forced_mode_uses_phone_ignoring_valid_hardware() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_PHONE_FORCED, true, true, 500);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_PHONE, s);
}

void test_phone_forced_mode_no_fix_when_phone_stale_even_if_hardware_valid() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_PHONE_FORCED, true, true, PHONE_FIX_STALE_MS + 1);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_NONE, s);
}

void test_phone_speed_computed_from_consecutive_samples() {
  // ~111m north in 10s (~0.001 deg latitude) -> ~40 km/h.
  const PhoneSpeedResult r = computePhoneSpeedKmh(0.0, 0.0, 1000, 0.001, 0.0, 11000);
  TEST_ASSERT_TRUE(r.speedValid);
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 40.0f, r.speedKmh);
}

void test_phone_speed_invalid_when_timestamp_does_not_advance() {
  const PhoneSpeedResult r = computePhoneSpeedKmh(0.0, 0.0, 1000, 0.001, 0.0, 1000);
  TEST_ASSERT_FALSE(r.speedValid);
}

void test_phone_speed_invalid_when_gap_too_large() {
  const PhoneSpeedResult r = computePhoneSpeedKmh(0.0, 0.0, 1000, 0.001, 0.0, 20000);
  TEST_ASSERT_FALSE(r.speedValid);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_hardware_mode_uses_hardware_when_valid);
  RUN_TEST(test_hardware_mode_falls_back_to_phone_when_hardware_invalid_and_phone_fresh);
  RUN_TEST(test_hardware_mode_ignores_stale_phone_fix);
  RUN_TEST(test_hardware_mode_no_fix_when_both_unavailable);
  RUN_TEST(test_phone_forced_mode_uses_phone_ignoring_valid_hardware);
  RUN_TEST(test_phone_forced_mode_no_fix_when_phone_stale_even_if_hardware_valid);
  RUN_TEST(test_phone_speed_computed_from_consecutive_samples);
  RUN_TEST(test_phone_speed_invalid_when_timestamp_does_not_advance);
  RUN_TEST(test_phone_speed_invalid_when_gap_too_large);
  return UNITY_END();
}
