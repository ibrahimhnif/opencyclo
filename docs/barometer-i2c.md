# Barometer shared-bus repair

Hardware diagnosis: BME280 at 0x76 consistently returned chip ID 0x60 through
LovyanGFX, while Arduino Wire read transactions failed at the same address.
Both drivers were accessing I2C controller 0 on SDA 16 / SCL 15.

The barometer now uses the vendored Bosch SensorAPI with LovyanGFX read/write
callbacks, protected by the same mutex as touch. Wire initialization and the
temporary Wire comparison probes are removed. BaroTask waits for display/touch
initialization before touching the bus. Compensation uses a single burst sample.
Transport errors never become accepted measurements. After eight failed readings,
initialization is retried after ten seconds; missing devices are probed every ten
seconds. Never hot-plug the hardware while powered.

The queue retains only the latest sample. Existing altitude smoothing, 2-second
freshness and GPS fallback are preserved. Altitude still assumes 1013.25 hPa;
it is not calibrated elevation. Diagnostic counters print every ten seconds.

Test: tests/test_bme280.cpp covers a reference compensation vector, wrong chip ID,
and failed transport propagation, and runs in tests/run_native_tests.py.

Live verification after flashing on 2026-09-11: valid=1, pressure around
1013.4 hPa, sensor temperature around 38.9 C, humidity around 52%, and zero
read errors/lock misses through 113 valid samples. Touch swipe events continued.
These measurements establish working communication, not calibrated accuracy.
