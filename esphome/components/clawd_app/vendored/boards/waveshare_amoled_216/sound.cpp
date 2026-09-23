#include "../../hal/sound_hal.h"

// ESPHome-build divergence from the PlatformIO firmware: on this board the
// ES8311 chime *is* wired up upstream (boards/waveshare_amoled_216/sound.cpp
// drives the shared ../../chime.cpp engine), but that engine needs Arduino's
// ESP_I2S library, which arduino-esp32 exposes no selective-compilation
// switch for — so it can't be built under ESPHome without patching the core
// (see VENDORED_EXCLUDED in components/clawd_app/__init__.py). Audio is
// therefore a no-op here, same posture as the 2.06 port, until either the
// chime is reimplemented on ESPHome's own i2s_audio component or upstream
// grows an ARDUINO_SELECTIVE_ESP_I2S symbol.

void sound_hal_init(void) {}
void sound_hal_tick(void) {}
void sound_hal_play_reset(void) {}
