#include "../../hal/display_hal.h"
#include "../../hal/imu_hal.h"
#include "../../brightness.h"
#include "board.h"
#include "../../hal/display_esphome.h"
#include "esphome/components/display/display.h"
#include <esp_heap_caps.h>
#include <lvgl.h>

// ESPHome-build display_hal implementation for waveshare_amoled_216.
//
// This file does NOT exist in the plain PlatformIO firmware — that build's
// boards/waveshare_amoled_216/display.cpp drives the CO5300 panel directly
// via Arduino_GFX_Library. Here, panel bring-up (QSPI bus, reset pulse, the
// CO5300 init sequence) is owned by ESPHome's `mipi_spi` component, which
// ships a WAVESHARE-ESP32-S3-TOUCH-AMOLED-2.16 model of its own — unlike the
// 2.06, this board needs no model patch. Everything below the panel driver —
// the software rotation strip fed by the IMU, the brightness ramp on a
// rotation change, the even-alignment requirement — is carried over from the
// PlatformIO driver unchanged, because it's panel-agnostic LVGL glue rather
// than CO5300 register work.
//
// The ClawdApp component calls display_hal_bind() once at startup with the
// display named by `clawd_app: display_id:` in YAML; ClawdApp runs at
// setup_priority::LATE so the display component's own setup() is done first.

#define ROT_BUF_LINES 40
static uint16_t* rot_buf = nullptr;

static esphome::display::Display* g_display = nullptr;
static std::function<void(uint8_t)> g_set_brightness;

void display_hal_bind(esphome::display::Display* disp,
                      std::function<void(uint8_t)> set_brightness) {
    g_display = disp;
    g_set_brightness = std::move(set_brightness);
}

void display_hal_init(void) {
    // No-op: the mipi_spi component already constructed the QSPI bus and
    // CO5300 driver by the time app_setup() runs.
}

void display_hal_begin(void) {
    if (!g_display) return;
    g_display->fill(esphome::Color(0, 0, 0));
    display_hal_set_brightness(200);

    rot_buf = (uint16_t*)heap_caps_malloc(LCD_WIDTH * ROT_BUF_LINES * 2,
                                          MALLOC_CAP_SPIRAM);
}

void display_hal_set_brightness(uint8_t level) {
    if (g_set_brightness) g_set_brightness(level);
}

void display_hal_fill_screen(uint16_t color565) {
    if (!g_display) return;
    uint8_t r = ((color565 >> 11) & 0x1F) << 3;
    uint8_t g = ((color565 >> 5) & 0x3F) << 2;
    uint8_t b = (color565 & 0x1F) << 3;
    g_display->fill(esphome::Color(r, g, b));
}

// Rotate a w×h strip into rot_buf and compute destination coordinates on the
// 480×480 panel. Src is row-major over the rectangle (sx, sy, w, h).
static void rotate_strip(const uint16_t* src, int32_t w, int32_t h,
                         int32_t sx, int32_t sy, uint8_t r,
                         int32_t* dx, int32_t* dy, int32_t* dw, int32_t* dh) {
    const int S = LCD_WIDTH;

    switch (r) {
    case 1: // 90° CW: (x,y) -> (S-1-y, x)
        *dw = h; *dh = w;
        *dx = S - sy - h;
        *dy = sx;
        for (int32_t y = 0; y < h; y++) {
            for (int32_t x = 0; x < w; x++) {
                rot_buf[x * h + (h - 1 - y)] = src[y * w + x];
            }
        }
        break;
    case 2: // 180°: (x,y) -> (S-1-x, S-1-y)
        *dw = w; *dh = h;
        *dx = S - sx - w;
        *dy = S - sy - h;
        for (int32_t y = 0; y < h; y++) {
            for (int32_t x = 0; x < w; x++) {
                rot_buf[(h - 1 - y) * w + (w - 1 - x)] = src[y * w + x];
            }
        }
        break;
    case 3: // 270° CW: (x,y) -> (y, S-1-x)
        *dw = h; *dh = w;
        *dx = sy;
        *dy = S - sx - w;
        for (int32_t y = 0; y < h; y++) {
            for (int32_t x = 0; x < w; x++) {
                rot_buf[(w - 1 - x) * h + y] = src[y * w + x];
            }
        }
        break;
    default:
        *dx = sx; *dy = sy; *dw = w; *dh = h;
        break;
    }
}

static void push_pixels(int32_t x, int32_t y, int32_t w, int32_t h,
                        const uint16_t* pixels) {
    // big_endian=true matches mipi_spi's default `byte_order` for QSPI
    // panels — verify on first hardware bring-up: swapped R/B channels or a
    // scrambled image means this needs to flip to false.
    g_display->draw_pixels_at(x, y, w, h,
                              reinterpret_cast<const uint8_t*>(pixels),
                              esphome::display::COLOR_ORDER_RGB,
                              esphome::display::COLOR_BITNESS_565,
                              /*big_endian=*/true);
}

void display_hal_draw_bitmap(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint16_t* pixels) {
    if (!g_display) return;
    uint8_t r = imu_hal_rotation_quadrant();
    if (r == 0 || !rot_buf) {
        push_pixels(x, y, w, h, pixels);
        return;
    }
    int32_t dx, dy, dw, dh;
    rotate_strip(pixels, w, h, x, y, r, &dx, &dy, &dw, &dh);
    push_pixels(dx, dy, dw, dh, rot_buf);
}

// On rotation change, blank the panel, force a full LVGL redraw at the new
// orientation, then ramp brightness back up over ~125ms so the transition
// reads as deliberate.
void display_hal_tick(void) {
    static uint8_t  last_rotation = 0;
    static uint8_t  ramp_step = 0;     // 0=idle, 1..4=ramping
    static uint32_t ramp_last = 0;

    uint8_t rot = imu_hal_rotation_quadrant();
    if (rot != last_rotation) {
        display_hal_set_brightness(0);
        last_rotation = rot;
        lv_obj_invalidate(lv_screen_active());
        ramp_step = 1;
        return;
    }

    if (ramp_step == 0) return;
    uint32_t now = millis();
    if (now - ramp_last < 25) return;
    ramp_last = now;

    static const uint8_t pct[] = {30, 60, 85, 100};
    uint8_t target = brightness_get();
    display_hal_set_brightness((uint8_t)(((uint16_t)target * pct[ramp_step - 1]) / 100));
    if (ramp_step >= 4) ramp_step = 0;
    else                ramp_step++;
}

// CO5300 requires even-aligned flush regions.
void display_hal_round_area(int32_t* x1, int32_t* y1, int32_t* x2, int32_t* y2) {
    *x1 = *x1 & ~1;
    *y1 = *y1 & ~1;
    *x2 = *x2 | 1;
    *y2 = *y2 | 1;
}
