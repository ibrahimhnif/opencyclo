#ifndef OPENCYCLO_CONFIG_PINS_H
#define OPENCYCLO_CONFIG_PINS_H

// ============================================================================
// OpenCyclo Pinout Configuration
// Target Board: lcdwiki 2.8inch ESP32-S3 Display (ES3C28P / ES3N28P)
// Single Source of Truth for hardware pin assignments
// ============================================================================

// Display (ILI9341 via SPI)
#define PIN_TFT_CS    10
#define PIN_TFT_DC    46
#define PIN_TFT_SCK   12
#define PIN_TFT_MOSI  11
#define PIN_TFT_MISO  13
#define PIN_TFT_BL    45
#define PIN_TFT_RST   -1  // Shared with EN / hardware reset line

// Touch Screen (FT6336G via I2C)
#define PIN_TOUCH_SDA 16
#define PIN_TOUCH_SCL 15
#define PIN_TOUCH_INT 17
#define PIN_TOUCH_RST 18
#define TOUCH_I2C_ADDR 0x38

// GPS Module (u-blox M10 via HardwareSerial)
// Board RX Pin: GPIO 44 (receives from GPS TX)
// Board TX Pin: GPIO 43 (transmits to GPS RX)
#define PIN_GPS_TX    43
#define PIN_GPS_RX    44
#define GPS_BAUD_RATE 115200

// External I2C (Barometer BMP280 - shares I2C bus with touch screen)
#define PIN_I2C_SDA   16
#define PIN_I2C_SCL   15

// microSD Card (SDIO Mode)
#define PIN_SD_CLK    38
#define PIN_SD_CMD    40
#define PIN_SD_D0     39
#define PIN_SD_D1     41
#define PIN_SD_D2     48
#define PIN_SD_D3     47

// Battery ADC
#define PIN_BATTERY_ADC 9

// Expansion GPIOs
#define PIN_EXP_1     2
#define PIN_EXP_2     3
#define PIN_EXP_3     14
#define PIN_EXP_4     21

#endif // OPENCYCLO_CONFIG_PINS_H
