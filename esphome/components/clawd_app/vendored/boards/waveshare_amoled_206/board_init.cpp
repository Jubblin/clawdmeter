#include "board.h"
#include <Arduino.h>
#include <Wire.h>

// AMOLED-2.06 has no IO expander. TP_RESET is a direct GPIO, pulsed here
// before touch_hal_init runs so the FT3168 is out of reset by the time its
// driver probes it.
//
// ESPHome-build divergence from the original board_init.cpp: LCD_RESET is
// NOT pulsed here. Under plain PlatformIO/Arduino_GFX this file owned that
// pin directly; in the ESPHome build the panel is brought up by the
// `mipi_spi` display component instead (see esphome/amoled_206.yaml,
// `reset_pin: GPIO8`), which pulses its own reset as part of the CO5300
// model's init sequence before this component's setup() runs (ClawdApp uses
// setup_priority::LATE — see clawd_app.h). Toggling GPIO8 from both places
// isn't unsafe, just redundant, so it's been dropped here to avoid two
// components racing to own the same physical pin.
extern "C" void board_init(void) {
    pinMode(TP_RESET,  OUTPUT);
    digitalWrite(TP_RESET,  LOW);
    delay(10);
    digitalWrite(TP_RESET,  HIGH);
    delay(50);

    Wire.begin(IIC_SDA, IIC_SCL);
}
