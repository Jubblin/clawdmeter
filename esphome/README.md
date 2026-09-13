# Clawdmeter on ESPHome — AMOLED-2.06 migration

This is a **parallel, experimental** ESPHome build for one board —
`waveshare_amoled_206` (410×502 watch form factor) — alongside the existing
PlatformIO firmware in `firmware/`, which keeps building and shipping
unchanged for all 7 hardware ports. See the plan this was built from for the
full rationale and phase breakdown.

## Status: compiles, not yet verified on hardware

`esphome compile esphome/amoled_206.yaml` builds a complete firmware image
(ESPHome 2026.8.2, Python 3.12) and CI runs it on every PR — so the YAML,
the custom component's codegen, and every vendored C++ file are known to
build against ESPHome's Arduino-as-IDF-component toolchain. Current cost of
the full app + NimBLE + WiFi/API/OTA: ~64% RAM, ~2.1 MB flash.

**Nothing here has run on hardware.** A green compile says nothing about
panel offsets, colour order, touch coordinates, PMU behaviour, BLE/WiFi
radio coexistence, or whether OTA works on this partition layout. What to
check on first flash:

1. `esphome run esphome/amoled_206.yaml --device /dev/ttyACM0`, then confirm
   the panel lights up, the splash renders centred (the `offset_width=23`
   viewport fix), and colours aren't byte-swapped (see Known gaps).
2. Touch coordinates against the BLE reset zone; PWR/PKEY short + long
   press; BOOT button → Space over BLE HID.
3. The daemon's BLE data channel, unmodified, against this build — then
   WiFi association and an OTA update while BLE is connected (NimBLE and
   WiFi share one radio; this is the most likely thing to misbehave).
4. Home Assistant discovery over the native API.

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
  (`esphome::display::Display::draw_pixels_at` / `fill`, plus a brightness
  callback bound in codegen because `mipi_spi`'s class is a template
  parameterised by the panel model).

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
in the same `h2zero/NimBLE-Arduino` library the PlatformIO firmware uses
(`^2.1.1` — see Known gaps), so `ble.cpp` ports with **zero changes**. Same reasoning for
`XPowersLib` (AXP2101 PMU) and `SensorLib` (QMI8658 IMU) — reusing
already-hardware-verified drivers is much lower risk than re-deriving
register-level behavior from a datasheet inside a new custom component.

## Building

ESPHome 2026.8.x needs **Python 3.12+**. `secrets.yaml` holds the WiFi
credentials, the API encryption key and the OTA password; it's gitignored,
so copy the example and fill it in (CI generates a throwaway one).

```bash
python3.12 -m venv ~/.venv-esphome && ~/.venv-esphome/bin/pip install 'esphome==2026.8.2'
esphome/patches/apply.sh ~/.venv-esphome/bin/python3     # see patches/README.md — re-run per venv
cp esphome/secrets.yaml.example esphome/secrets.yaml     # then edit it
~/.venv-esphome/bin/esphome compile esphome/amoled_206.yaml
~/.venv-esphome/bin/esphome run esphome/amoled_206.yaml --device /dev/ttyACM0   # flash
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
  Out of scope for this migration's v1. The shared `chime.cpp` engine is
  excluded from the build entirely (`VENDORED_EXCLUDED` in the component's
  `__init__.py`): it needs Arduino's `ESP_I2S` library, and arduino-esp32
  exposes no selective-compilation switch for that one, so it cannot be
  enabled under ESPHome's Arduino build without patching the core.
- **Library versions** are semver ranges, not the exact pins
  `firmware/platformio.ini` uses: ESPHome resolves them through the
  PlatformIO registry, which doesn't carry every upstream patch release
  (NimBLE-Arduino 2.1.1 among them).
- **PCF85063 RTC**: not wired up at all (matches upstream — unused feature).
- The CO5300 model's `offset_width=23` and `reset_pin=8` came straight from
  the hardware-verified PlatformIO driver; nothing here should need
  re-deriving, only re-verifying once compiled and flashed.
