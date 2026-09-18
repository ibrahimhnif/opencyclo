# SD mount recovery

Logger now attempts mount under the shared SD lock, once per five-second
interval until successful: 4-bit 20 MHz, 4-bit 10 MHz, then 1-bit 10 MHz.
Formatting is always disabled. Successful mount updates map availability and
telemetry; it is never automatically unmounted. Shutdown, an open GPX file and
a failed save awaiting explicit retry exclude recovery. This does not implement
safe hot removal or recovery from removal of an already-mounted card.

`python3 tests/run_native_tests.py` checks retry spacing, mode fallback,
exclusion, success latching and timer wrap. Navigation integration tests pass.

Hardware observation after upload: initial 4-bit 20 MHz and subsequent retries,
including 1-bit 10 MHz, fail with `sdmmc_init_ocr: send_op_cond (1) returned
0x107` / `sdmmc_card_init failed`. In the installed ESP-IDF headers 0x107 means
ESP_ERR_TIMEOUT. This is card initialization, before filesystem mounting; it
does not establish filesystem corruption. Concurrent I2C read errors were also
observed, but do not establish a shared cause.

Next physical isolation: fully remove power, temporarily disconnect the newly
attached barometer, check/reseat the card, then power up and compare the mount
log. Inspect actual sensor wiring, shorts/contact and supply before changing
SD pins or formatting anything. Configured SD pins are CLK38 CMD40 D0=39 D1=41
D2=48 D3=47; barometer/touch I2C is SDA16 SCL15.

No existing map or GPX files were deleted or formatted during this work.
