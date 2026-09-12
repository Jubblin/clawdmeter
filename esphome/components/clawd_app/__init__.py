import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components.mipi_spi.display import MipiSpi

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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    disp = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(disp))

    # The vendored app (ble.cpp) needs NimBLE-Arduino + Preferences (NVS) for
    # BLE HID + the single-owner bond lock; ArduinoJson for the daemon's
    # usage payload; the same LVGL version the rest of the vendored UI code
    # was written against. Pinned to match firmware/platformio.ini exactly.
    cg.add_library("h2zero/NimBLE-Arduino", "2.1.1")
    cg.add_library("bblanchon/ArduinoJson", "7.0.0")
    cg.add_library("lvgl/lvgl", "9.2.0")
    cg.add_library("lewisxhe/SensorLib", "0.2.6")
    cg.add_library("lewisxhe/XPowersLib", "0.2.7")

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
