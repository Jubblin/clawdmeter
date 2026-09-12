#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mipi_spi/mipi_spi.h"

// The whole Clawdmeter application (BLE, UI, splash animations, idle/
// brightness, usage-rate tracking, button handling) is vendored under
// vendored/ almost verbatim from firmware/src/ — see vendored/app_main.cpp
// (renamed from main.cpp's setup()/loop() to avoid colliding with
// ESPHome's own generated entry points) and README.md at the repo root of
// this esphome/ directory for what's vendored vs. new.
//
// ClawdApp is the one genuinely new piece: a thin ESPHome Component whose
// setup()/loop() just call into that vendored code, and which hands the
// vendored display_hal adapter a pointer to the mipi_spi display object
// once ESPHome has finished bringing up the panel.
extern void app_setup();
extern void app_loop();

namespace esphome::clawd_app {

class ClawdApp : public Component {
 public:
  void set_display(mipi_spi::MipiSpi *disp) { this->display_ = disp; }

  void setup() override;
  void loop() override { app_loop(); }

  // Must run after the display component's own setup() (which pulses
  // reset and runs the CO5300 init sequence) and after i2c/board_init's
  // Wire.begin() — LATE keeps this comfortably behind ESPHome's hardware
  // setup_priority tiers.
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  mipi_spi::MipiSpi *display_{nullptr};
};

}  // namespace esphome::clawd_app
