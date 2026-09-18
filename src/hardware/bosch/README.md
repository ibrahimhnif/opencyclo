# Bosch BME280 SensorAPI

Vendored from https://github.com/boschsensortec/BME280_SensorAPI
at c90d419492e26dd95586598a794e65eb2760753a.
Source unchanged except normalized line endings. BSD-3-Clause license included.

OpenCyclo uses the double-precision compensation default and supplies LovyanGFX
I2C callbacks in baro_task.cpp. Do not reintroduce Arduino Wire on this shared
touch bus: hardware diagnostics showed Wire reads failing while LGFX reads
returned the correct BME280 ID (0x60 at 0x76).
