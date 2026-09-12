#include "../../hal/display_hal.h"
#include "display_esphome.h"
#include "esphome/components/mipi_spi/mipi_spi.h"

// ESPHome-build display_hal implementation for waveshare_amoled_206.
//
// This file does NOT exist in the plain PlatformIO firmware — that build's
// boards/waveshare_amoled_206/display.cpp drives the CO5300 panel directly
// via Arduino_GFX_Library. Here, panel bring-up (QSPI bus, reset pulse, the
// CO5300 SLPOUT/PAGESEL/WRCTRLD/WCE init sequence, and the col_offset1=23
// GRAM-viewport centering) is instead owned by ESPHome's `mipi_spi` display
// component (see esphome/patches/ — it adds a WAVESHARE-ESP32-S3-TOUCH-
// AMOLED-2.06 model to that component's CO5300 family). This file only
// forwards the shared HAL calls (used by main.cpp's LVGL glue and by
// idle.cpp/brightness.cpp) onto that already-initialized display object.
//
// The ClawdApp component (clawd_app.cpp) calls display_hal_set_display()
// once at startup with the `mipi_spi::MipiSpi*` instance named in YAML via
// `clawd_app: display_id:`. It must run after the display component's own
// setup() — see ClawdApp::get_setup_priority() (setup_priority::LATE).

static esphome::mipi_spi::MipiSpi* g_display = nullptr;

void display_hal_set_display(esphome::mipi_spi::MipiSpi* disp) {
    g_display = disp;
}

void display_hal_init(void) {
    // No-op: the mipi_spi component already constructed the QSPI bus and
    // CO5300 driver by the time app_setup() runs.
}

void display_hal_begin(void) {
    if (!g_display) return;
    // The mipi_spi component already ran the panel's init sequence and
    // pulsed reset; this just matches the original display_hal_begin()
    // contract (clear + default brightness) before LVGL starts drawing.
    g_display->fill(esphome::Color(0, 0, 0));
    g_display->set_brightness(200);
}

void display_hal_set_brightness(uint8_t level) {
    if (g_display) g_display->set_brightness(level);
}

void display_hal_fill_screen(uint16_t color565) {
    if (!g_display) return;
    uint8_t r = ((color565 >> 11) & 0x1F) << 3;
    uint8_t g = ((color565 >> 5) & 0x3F) << 2;
    uint8_t b = (color565 & 0x1F) << 3;
    g_display->fill(esphome::Color(r, g, b));
}

void display_hal_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint16_t* pixels) {
    if (!g_display) return;
    // big_endian=true matches mipi_spi's default `byte_order` for QSPI
    // panels (see esphome/components/mipi_spi/display.py CONF_BYTE_ORDER) —
    // verify on first hardware bring-up: swapped R/B channels or a
    // scrambled/striped image means this needs to flip to false.
    g_display->draw_pixels_at(x, y, w, h, reinterpret_cast<const uint8_t*>(pixels),
                              esphome::display::COLOR_ORDER_RGB,
                              esphome::display::COLOR_BITNESS_565,
                              /*big_endian=*/true);
}

void display_hal_tick(void) {
    // No rotation handling on this board (fixed watch-enclosure orientation).
}

// CO5300 requires even-aligned flush regions — same requirement as the
// PlatformIO build's display.cpp.
void display_hal_round_area(int32_t* x1, int32_t* y1, int32_t* x2, int32_t* y2) {
    *x1 = *x1 & ~1;
    *y1 = *y1 & ~1;
    *x2 = *x2 | 1;
    *y2 = *y2 | 1;
}
