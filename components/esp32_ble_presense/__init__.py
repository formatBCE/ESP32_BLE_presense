# SPDX-License-Identifier: AGPL-3.0-only
# (C) Copyright 2024 - Greg Whiteley

import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components import time

from esphome.const import (
    CONF_ID,
    CONF_ON_UPDATE,
)

CONF_HEARTBEAT = "on_heartbeat"

CODEOWNERS = ["@formatBCE"]

ESP32_BLE_Presense_ns = cg.esphome_ns.namespace("esp32_ble_presense")
ESP32_BLE_Presense = ESP32_BLE_Presense_ns.class_(
    "ESP32_BLE_Presense",
    cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESP32_BLE_Presense),
    cv.Required(CONF_ON_UPDATE): automation.validate_automation(single=True),
    cv.Required(CONF_HEARTBEAT): automation.validate_automation(single=True),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    cg.add_library(
        name="NimBLE-Arduino",
        version="1.4.2",
    )
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await automation.build_automation(
            var.on_update(), [(cg.std_string, "uid"), (cg.int32, "rssi")], config[CONF_ON_UPDATE]
    )
    await automation.build_automation(
            var.on_heartbeat(), [], config[CONF_HEARTBEAT]
    )
