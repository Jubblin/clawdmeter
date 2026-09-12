# Registers a WAVESHARE-ESP32-S3-TOUCH-AMOLED-2.06 model with ESPHome's
# mipi_spi component, alongside the WAVESHARE-ESP32-S3-TOUCH-AMOLED-2.16 and
# -1.75 models that ship in esphome/components/mipi_spi/models/waveshare.py
# (added for the 2.16 in ESPHome 2026.6.0, PR esphome/esphome#16887).
#
# Not upstreamed (yet) — see esphome/patches/apply.sh for why this lives as a
# drop-in file instead of a vendored copy of the whole mipi_spi component.
#
# offset_width=23 is this board's col_offset1 value from the plain
# PlatformIO firmware (firmware/src/boards/waveshare_amoled_206/display.cpp):
# the 2.06's 410-wide viewport sits 23 columns into the CO5300 controller's
# wider internal GRAM. Without it, a vertical strip of stale/garbage content
# shows through on the right edge — see that file's comment for the full
# story (23 was picked empirically; Waveshare's own reference library uses
# 22). offset_height=0 — rows are already aligned.
#
# reset_pin=8 is a direct GPIO on this board (no IO expander), unlike the
# 2.16 which resets via GPIO 39.
from esphome.components.mipi_spi.models.amoled import CO5300

CO5300.extend(
    "WAVESHARE-ESP32-S3-TOUCH-AMOLED-2.06",
    width=410,
    height=502,
    pixel_mode="16bit",
    offset_width=23,
    offset_height=0,
    cs_pin=12,
    reset_pin=8,
    data_rate="40MHz",
)
