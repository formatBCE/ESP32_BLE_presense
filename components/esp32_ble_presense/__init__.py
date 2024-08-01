# SPDX-License-Identifier: AGPL-3.0-only
# (C) Copyright 2024 - Greg Whiteley

import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components import time

from esphome.const import (
    CONF_ID,
    CONF_AREA,
    CONF_TIME_ID,
    CONF_ON_UPDATE,
    CONF_TRIGGER_ID,
)

CODEOWNERS = ["@formatBCE"]

ESP32_BLE_Presense_ns = cg.esphome_ns.namespace("esp32_ble_presense")
ESP32_BLE_Presense = ESP32_BLE_Presense_ns.class_(
    "ESP32_BLE_Presense",
    cg.PollingComponent,
    cg.esphome_ns.namespace("mqtt").class_("CustomMQTTDevice")
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESP32_BLE_Presense),
    cv.Required(CONF_AREA): cv.string,
    cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    cv.Required(CONF_ON_UPDATE): automation.validate_automation(single=True),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    cg.add_library(
        name="NimBLE-Arduino",
        version="1.4.2",
    )
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_area(config[CONF_AREA]))

    if CONF_TIME_ID in config:
        time_ = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time(time_))

    await automation.build_automation(
            var.on_update(), [(cg.std_string, "id"), (cg.int8, "rssi"), (cg.uint32, "timestamp")], config[CONF_ON_UPDATE]
        )
