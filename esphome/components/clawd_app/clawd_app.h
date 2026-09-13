#pragma once

#include <functional>
#include <utility>

#include "esphome/core/component.h"
#include "esphome/components/display/display.h"

// The whole Clawdmeter application (BLE, UI, splash animations, idle/
// brightness, usage-rate tracking, button handling) is vendored under
// vendored/ almost verbatim from firmware/src/ — see vendored/app_main.cpp
// (renamed from main.cpp's setup()/loop() to avoid colliding with
// ESPHome's own generated entry points) and README.md at the repo root of
// this esphome/ directory for what's vendored vs. new.
//
// ClawdApp is the one genuinely new piece: a thin ESPHome Component whose
// setup()/loop() just call into that vendored code, and which hands the
// vendored display_hal adapter a pointer to the display object once ESPHome
// has finished bringing up the panel.
extern void app_setup();
extern void app_loop();

namespace esphome::clawd_app {

class ClawdApp : public Component {
 public:
  void set_display(display::Display *disp, std::function<void(uint8_t)> set_brightness) {
    this->display_ = disp;
    this->set_brightness_ = std::move(set_brightness);
  }

  void setup() override;
  void loop() override { app_loop(); }

  // Must run after the display component's own setup() (which pulses
  // reset and runs the CO5300 init sequence) and after i2c/board_init's
  // Wire.begin() — LATE keeps this comfortably behind ESPHome's hardware
  // setup_priority tiers.
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  display::Display *display_{nullptr};
  std::function<void(uint8_t)> set_brightness_{};
};

// Brightness lives on mipi_spi::MipiSpi, a class template whose arguments
// depend on the panel model, so the concrete type is only known in the
// generated setup code — deduce it there instead of naming it here.
template<typename T> void bind_display(ClawdApp *app, T *disp) {
  app->set_display(disp, [disp](uint8_t level) { disp->set_brightness(level); });
}

}  // namespace esphome::clawd_app
