#include "clawd_app.h"
#include "vendored/boards/waveshare_amoled_206/display_esphome.h"

namespace esphome::clawd_app {

void ClawdApp::setup() {
  display_hal_bind(this->display_, this->set_brightness_);
  app_setup();
}

}  // namespace esphome::clawd_app
