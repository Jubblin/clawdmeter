from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.mipi_spi.display import MipiSpi
from esphome.core import CORE
from esphome.helpers import copy_file_if_changed

CODEOWNERS = ["@jubblin"]

clawd_app_ns = cg.esphome_ns.namespace("clawd_app")
ClawdApp = clawd_app_ns.class_("ClawdApp", cg.Component)

CONF_DISPLAY_ID = "display_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ClawdApp),
        cv.Required(CONF_DISPLAY_ID): cv.use_id(MipiSpi),
    }
).extend(cv.COMPONENT_SCHEMA)


VENDORED_DIR = Path(__file__).parent / "vendored"
VENDORED_SUFFIXES = (".c", ".cpp", ".h", ".hpp")
# The shared ES8311 chime engine needs Arduino's ESP_I2S library, which
# arduino-esp32 doesn't expose a selective-compilation switch for, and this
# board's sound_hal is a no-op anyway (vendored/boards/.../sound.cpp).
VENDORED_EXCLUDED = frozenset({"chime.cpp", "chime.h"})


def _copy_vendored_sources() -> None:
    """Mirror vendored/ into the build's src/ tree.

    ESPHome only picks up source files sitting directly in an external
    component's directory, so the vendored firmware — which is a directory
    tree — has to be copied in by hand. src/ is globbed recursively by the
    generated CMakeLists, and `#include "vendored/..."` resolves against it.
    """
    dest_root = Path(CORE.relative_src_path("vendored"))
    for src in sorted(VENDORED_DIR.rglob("*")):
        if not src.is_file() or src.suffix not in VENDORED_SUFFIXES:
            continue
        if src.name in VENDORED_EXCLUDED:
            continue
        dest = dest_root / src.relative_to(VENDORED_DIR)
        dest.parent.mkdir(parents=True, exist_ok=True)
        copy_file_if_changed(src, dest)


async def to_code(config):
    _copy_vendored_sources()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    disp = await cg.get_variable(config[CONF_DISPLAY_ID])
    # bind_display() is a function template: the mipi_spi display's concrete
    # type (it's a class template parameterised by the panel model) is only
    # known to the compiler, not here.
    cg.add(clawd_app_ns.bind_display(var, disp))

    # The vendored app (ble.cpp) needs NimBLE-Arduino + Preferences (NVS) for
    # BLE HID + the single-owner bond lock; ArduinoJson for the daemon's
    # usage payload; the same LVGL version the rest of the vendored UI code
    # was written against. Ranges match firmware/platformio.ini's lib_deps;
    # exact pins can't be used because ESPHome resolves these through the
    # PlatformIO registry, which doesn't carry every upstream patch release
    # (e.g. NimBLE-Arduino 2.1.1).
    cg.add_library("h2zero/NimBLE-Arduino", "^2.1.1")
    cg.add_library("bblanchon/ArduinoJson", "^7.0.0")
    cg.add_library("lvgl/lvgl", "^9.2.0")
    cg.add_library("lewisxhe/SensorLib", "^0.2.6")
    cg.add_library("lewisxhe/XPowersLib", "^0.2.7")

    # ESPHome builds arduino-esp32 with selective compilation and only keeps
    # the Arduino libraries someone asked for: Wire/SPI for the sensor and PMU
    # drivers, Preferences (NVS) for the BLE owner lock.
    for arduino_lib in ("Wire", "SPI", "Preferences"):
        cg.add_library(arduino_lib, None)

    # NimBLE peripheral-only role + connection-parameter tuning — see the
    # onConnParamsUpdate comment in vendored/ble.cpp for why the PPCP values
    # matter (Windows supervision-timeout workaround). Matches
    # firmware/platformio.ini's [env:waveshare_amoled_206] build_flags.
    for flag in (
        "-DBOARD_HAS_PSRAM",
        "-DXPOWERS_CHIP_AXP2101",
        "-DCONFIG_BT_NIMBLE_ROLE_BROADCASTER=1",
        "-DCONFIG_BT_NIMBLE_ROLE_PERIPHERAL=1",
        "-DCONFIG_BT_NIMBLE_ROLE_CENTRAL=0",
        "-DCONFIG_BT_NIMBLE_ROLE_OBSERVER=0",
        "-DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=2",
        "-DMYNEWT_VAL_BLE_SVC_GAP_PPCP_MIN_CONN_INTERVAL=12",
        "-DMYNEWT_VAL_BLE_SVC_GAP_PPCP_MAX_CONN_INTERVAL=24",
        "-DMYNEWT_VAL_BLE_SVC_GAP_PPCP_SLAVE_LATENCY=0",
        "-DMYNEWT_VAL_BLE_SVC_GAP_PPCP_SUPERVISION_TMO=600",
        "-DLV_CONF_SKIP",
        "-DLV_COLOR_DEPTH=16",
        "-DLV_USE_LOG=0",
        "-DLV_USE_BAR=1",
        "-DLV_USE_LABEL=1",
        "-DLV_USE_ARC=1",
        "-DLV_USE_BTN=1",
        "-DLV_USE_LINE=1",
        "-DLV_USE_OBJ=1",
        "-DLV_USE_IMG=1",
        "-DLV_USE_IMAGE=1",
        "-DLV_USE_ANIMIMG=0",
        "-DLV_TICK_CUSTOM=1",
        "-DLV_USE_SNAPSHOT=1",
    ):
        cg.add_build_flag(flag)
