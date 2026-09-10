#include "display.h"

LGFX tft;
LGFX_Sprite canvas(&tft);

LGFX::LGFX() {
  {
    auto cfg = _bus_instance.config();
    cfg.spi_host = SPI2_HOST;
    cfg.spi_mode = 0;
    // Diagnostic baseline for intermittent whole-screen horizontal shifts.
    // Change only the SPI write clock (previously 80 MHz), keeping rendering
    // and frame pacing unchanged so a physical A/B test isolates this factor.
    // A full-frame transfer will take longer; stability is not yet verified.
    cfg.freq_write = 40000000;
    cfg.freq_read  = 16000000;
    cfg.spi_3wire  = false;
    cfg.use_lock   = true;
    cfg.pin_sclk   = PIN_TFT_SCK;  // 12
    cfg.pin_mosi   = PIN_TFT_MOSI; // 11
    cfg.pin_miso   = PIN_TFT_MISO; // 13
    cfg.pin_dc     = PIN_TFT_DC;   // 46
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);
  }

  {
    auto cfg = _panel_instance.config();
    cfg.pin_cs           = PIN_TFT_CS;  // 10
    cfg.pin_rst          = PIN_TFT_RST; // -1
    cfg.pin_busy         = -1;
    cfg.panel_width      = 240;
    cfg.panel_height     = 320;
    cfg.offset_x         = 0;
    cfg.offset_y         = 0;
    cfg.offset_rotation  = 0;
    cfg.dummy_read_pixel = 8;
    cfg.readable         = true;
    cfg.invert           = true;
    cfg.rgb_order        = false;
    cfg.dlen_16bit       = false;
    cfg.bus_shared       = false;
    _panel_instance.config(cfg);
  }

  {
    auto cfg = _light_instance.config();
    cfg.pin_bl = PIN_TFT_BL; // 45
    cfg.invert = false;
    cfg.freq   = 44100;
    cfg.pwm_channel = 7;
    _light_instance.config(cfg);
    _panel_instance.setLight(&_light_instance);
  }

  {
    auto cfg = _touch_instance.config();
    cfg.x_min      = 0;
    cfg.x_max      = 239;
    cfg.y_min      = 0;
    cfg.y_max      = 319;
    cfg.pin_sda    = PIN_TOUCH_SDA; // 16
    cfg.pin_scl    = PIN_TOUCH_SCL; // 15
    cfg.pin_int    = PIN_TOUCH_INT; // 17
    cfg.pin_rst    = PIN_TOUCH_RST; // 18
    cfg.i2c_port   = 0;
    cfg.i2c_addr   = TOUCH_I2C_ADDR; // 0x38
    cfg.freq       = 400000;
    cfg.offset_rotation = 2;
    cfg.bus_shared = true;
    _touch_instance.config(cfg);
    _panel_instance.setTouch(&_touch_instance);
  }

  setPanel(&_panel_instance);
}

#include <Wire.h>

void initDisplay() {
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 400000);
  tft.init();
  tft.setRotation(2); // Flipped 180 degrees so USB connector is at the bottom
  tft.setBrightness(200);
  tft.fillScreen(TFT_BLACK);

  canvas.setPsram(true);
  canvas.setColorDepth(16); // RGB565, matches the panel's native depth
  if (!canvas.createSprite(240, 320)) {
    // ~150KB PSRAM allocation failing would be a real hardware/config
    // problem (this board has 8MB) -- log it loudly rather than silently
    // rendering nothing, since every widget draws to canvas from here on.
    Serial.println("[DISPLAY] FATAL: canvas.createSprite() failed -- out of PSRAM?");
  }
  canvas.fillScreen(TFT_BLACK);
}

void setDisplayBrightness(uint8_t duty) {
  tft.setBrightness(duty);
}
