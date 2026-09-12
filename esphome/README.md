# Clawdmeter on ESPHome — AMOLED-2.06 migration

This is a **parallel, experimental** ESPHome build for one board —
`waveshare_amoled_206` (410×502 watch form factor) — alongside the existing
PlatformIO firmware in `firmware/`, which keeps building and shipping
unchanged for all 7 hardware ports. See the plan this was built from for the
full rationale and phase breakdown.

## Status: scaffolded, not yet verified on hardware

Everything below has been validated with `esphome config` (schema/codegen
validation, including a real compile-time check that the CO5300 model patch
loads and the custom component's Python side is correct) in this repo's dev
environment. A full `esphome compile` could not be completed in the sandbox
this was built in — PlatformIO's `pioarduino` toolchain needs to (re)fetch a
Python dependency (`platformio` itself) from a GitHub release archive, and
that host is blocked by this environment's egress policy. That failure is
environmental, not a defect in this config — **the actual C++ compile has
not been exercised**, and nothing here has touched real hardware. Treat this
as a structurally-complete first draft, not a working build. Next steps for
whoever picks this up:

1. `pip install esphome` (or a venv — see below) somewhere with normal
   internet access, run `esphome compile esphome/amoled_206.yaml`, and fix
   whatever C++ errors the vendored/adapter code turns up. None of this has
   compiled yet.
2. Flash to a real AMOLED-2.06 board and work through the phases and
   Verification checklist in the plan this was built from (display bring-up,
   touch, PMU/PKEY gestures, BLE data channel against the unmodified daemon,
   BLE HID keyboard across OSes, splash animations).

## Layout

```text
esphome/
  amoled_206.yaml              — the ESPHome device config
  patches/                     — see patches/README.md: adds a CO5300 model
                                  for this board to ESPHome's mipi_spi component
  components/clawd_app/
    __init__.py, clawd_app.h/.cpp   — the one genuinely new piece (see below)
    vendored/                       — copied from firmware/src/, see below
```

## What's vendored vs. new

Most of `components/clawd_app/vendored/` is a **verbatim or near-verbatim
copy** of `firmware/src/` — the application logic (BLE data channel + HID
keyboard, LVGL UI, splash pixel-art animation engine, idle/brightness,
usage-rate tracking, the JSON parsing loop) doesn't know or care that it's
running under ESPHome instead of plain PlatformIO, so it was copied rather
than rewritten. Specifically:

- `ui.*`, `splash.*`, `splash_animations.h`, `splash_geometry.h`, `theme.h`,
  `usage_rate.*`, `idle.*`, `idle_cfg.h`, `brightness.*`, `chime.*`, `data.h`,
  `ble.*`, `icons.h`, `logo.h`, `clawd_still.h`, `font_*.c` — unmodified
  copies of the shared, board-agnostic files.
- `hal/*.h` — unmodified copies of the HAL interfaces (`display_hal.h`,
  `touch_hal.h`, `input_hal.h`, `power_hal.h`, `imu_hal.h`, `sound_hal.h`,
  `board_caps.h`). The HAL abstraction from the PlatformIO firmware is kept
  intact — this build implements it against ESPHome/mipi_spi instead of
  discarding it.
- `boards/waveshare_amoled_206/{board.h,caps.cpp,touch.cpp,power.cpp,
  imu.cpp,sound.cpp,input.cpp}` — unmodified copies. These are already plain
  Arduino/Wire/XPowersLib/SensorLib code with no Arduino_GFX or ESPHome
  dependency, so they needed no adapter at all.
- `boards/waveshare_amoled_206/board_init.cpp` — copied with **one
  deliberate edit**: the LCD_RESET pulse was removed (see the comment in
  that file) because `mipi_spi`'s own `reset_pin` config now owns that pin.
  TP_RESET (touch) is still pulsed here exactly as before.
- `app_main.cpp` — `firmware/src/main.cpp`, copied with **one mechanical
  rename**: `void setup()` / `void loop()` → `void app_setup()` /
  `void app_loop()`, so they don't collide with ESPHome's own generated
  Arduino entry points. No other change.
- `boards/waveshare_amoled_206/display.cpp` — **new file, not vendored**.
  The original used Arduino_GFX_Library + `Arduino_CO5300` directly; this
  build instead lets ESPHome's `mipi_spi` component own panel bring-up (see
  patches/README.md) and this file just forwards the same `display_hal_*`
  calls onto that already-initialized display object
  (`esphome::mipi_spi::MipiSpi::draw_pixels_at` / `set_brightness` / `fill`).

`components/clawd_app/__init__.py` + `clawd_app.h/.cpp` are the one
genuinely new ESPHome component: it's a thin `Component` whose `setup()`
hands the display adapter a pointer to the `mipi_spi` display object (see
`display_id:` in the YAML) and calls `app_setup()`, and whose `loop()` calls
`app_loop()`. Everything else — BLE, UI, splash, buttons, power — happens
inside the vendored code exactly as it did under PlatformIO.

## Why NimBLE-Arduino instead of `esp32_ble_server` / a rewritten BLE stack

`ble.cpp` (custom data-channel GATT service + BLE HID keyboard + the
single-owner bond lock + the Windows connection-timeout workaround) was
identified up front as the highest-risk part of this migration — it's
~460 lines of NimBLE-specific logic with no ESPHome-native equivalent.
Rather than reimplement it against `esp32_ble_server`'s more limited YAML
surface (which doesn't expose bond pruning, `onAuthenticationComplete`, or
raw `updateConnParams()`), this build keeps `framework: arduino` (verified
via `esphome config` — `mipi_spi` has no esp-idf-only restriction) and pulls
in the exact same `h2zero/NimBLE-Arduino@2.1.1` library the PlatformIO
firmware uses, so `ble.cpp` ports with **zero changes**. Same reasoning for
`XPowersLib` (AXP2101 PMU) and `SensorLib` (QMI8658 IMU) — reusing
already-hardware-verified drivers is much lower risk than re-deriving
register-level behavior from a datasheet inside a new custom component.

## Building

```bash
python3 -m venv /tmp/esphome-venv && /tmp/esphome-venv/bin/pip install esphome
esphome/patches/apply.sh /tmp/esphome-venv/bin/python3   # see patches/README.md
/tmp/esphome-venv/bin/esphome config esphome/amoled_206.yaml    # validates cleanly
/tmp/esphome-venv/bin/esphome compile esphome/amoled_206.yaml   # not yet exercised end-to-end, see Status
/tmp/esphome-venv/bin/esphome run esphome/amoled_206.yaml --device /dev/ttyACM0   # flash
```

## Known gaps / things the next pass should check first

- **Byte order for `draw_pixels_at`** (`display.cpp`): set to `big_endian`
  to match `mipi_spi`'s default for QSPI panels. Verify on first boot —
  swapped colors or a scrambled image means flip it to `false`.
- **QMI8658 IMU**: vendored `imu.cpp` initializes it for bus health only
  (matches the PlatformIO firmware's posture — rotation is disabled on this
  watch-enclosure board either way). Untouched, no known risk.
- **ES8311 chime**: vendored `sound.cpp` no-ops, matching the PlatformIO
  firmware's own posture on this board (amp path unverified in hardware).
  Out of scope for this migration's v1.
- **PCF85063 RTC**: not wired up at all (matches upstream — unused feature).
- The CO5300 model's `offset_width=23` and `reset_pin=8` came straight from
  the hardware-verified PlatformIO driver; nothing here should need
  re-deriving, only re-verifying once compiled and flashed.
