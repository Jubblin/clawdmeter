#pragma once

#include <cstdint>
#include <functional>

// Not part of the shared hal/display_hal.h contract (that header is vendored
// unmodified from firmware/src/hal/). This is the one extra hook the
// ESPHome build needs: a way for ClawdApp to hand this file's display.cpp
// the already-initialized display object before app_setup() runs.
//
// Brightness comes in as a callback because it lives on the mipi_spi
// display template, not on the generic display::Display interface.

namespace esphome::display {
class Display;
}

void display_hal_bind(esphome::display::Display* disp,
                      std::function<void(uint8_t)> set_brightness);
