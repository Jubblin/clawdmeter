#pragma once

// Not part of the shared hal/display_hal.h contract (that header is vendored
// unmodified from firmware/src/hal/). This is the one extra hook the
// ESPHome build needs: a way for ClawdApp to hand this file's display.cpp
// the already-initialized mipi_spi display object before app_setup() runs.

namespace esphome::mipi_spi {
class MipiSpi;
}

void display_hal_set_display(esphome::mipi_spi::MipiSpi* disp);
