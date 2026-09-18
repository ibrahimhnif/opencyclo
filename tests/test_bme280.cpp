#include "../src/hardware/bosch/bme280.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
static bool failRead = false;
static int8_t readReg(uint8_t reg, uint8_t* out, uint32_t n, void*) {
  if (failRead) return -1;
  memset(out, 0, n);
  if (reg == 0xD0) out[0] = 0x58; // Wrong chip must not initialize.
  return 0;
}
static int8_t writeReg(uint8_t, const uint8_t*, uint32_t, void*) { return 0; }
static void delayUs(uint32_t, void*) {}
int main() {
  bme280_calib_data c{};
  c.dig_t1=27504; c.dig_t2=26435; c.dig_t3=-1000;
  c.dig_p1=36477; c.dig_p2=-10685; c.dig_p3=3024;
  c.dig_p4=2855; c.dig_p5=140; c.dig_p6=-7;
  c.dig_p7=15500; c.dig_p8=-14600; c.dig_p9=6000;
  bme280_uncomp_data raw{};
  raw.temperature=519888; raw.pressure=415148;
  bme280_data out{};
  assert(bme280_compensate_data(BME280_PRESS | BME280_TEMP, &raw, &out, &c)==BME280_OK);
  assert(std::abs(out.temperature-25.08)<0.02);
  assert(std::abs(out.pressure-100653.27)<1.0);
  bme280_dev dev{};
  dev.intf=BME280_I2C_INTF; dev.read=readReg; dev.write=writeReg; dev.delay_us=delayUs;
  assert(bme280_init(&dev)==BME280_E_DEV_NOT_FOUND);
  failRead=true;
  assert(bme280_init(&dev)==BME280_E_COMM_FAIL);
  assert(bme280_get_sensor_data(BME280_ALL, &out, &dev)==BME280_E_COMM_FAIL);
  puts("Bosch compensation, wrong-chip and communication failure tests passed");
}
