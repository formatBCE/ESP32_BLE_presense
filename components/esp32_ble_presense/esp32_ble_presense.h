////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: AGPL-3.0-only
// (C) Copyright 2022-2024 - formatBCE

#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace esp32_ble_presense {

class ESP32_BLE_Presense : public esphome::PollingComponent {

    std::string name;

    std::vector<std::string> macs;
    std::vector<std::string> uuids;

public:

    ESP32_BLE_Presense();
    ~ESP32_BLE_Presense() = default;

    void update() override;
    void setup() override;

    void add_beacon(const std::string& uid);
    void remove_beacon(const std::string& uid);

    Trigger<> *on_heartbeat() const { return this->on_heartbeat_trigger_; }
    Trigger<std::string, int32_t> *on_update() const { return this->on_update_trigger_; }

    ESP32_BLE_Presense(const ESP32_BLE_Presense&) = delete;
    ESP32_BLE_Presense& operator=(const ESP32_BLE_Presense&) = delete;

protected:
    Trigger<> *on_heartbeat_trigger_ = new Trigger<>();
    Trigger<std::string, int32_t> *on_update_trigger_ = new Trigger<std::string, int32_t>();

    friend class BleAdvertisedDeviceCallbacks;
    void reportDevice(const std::string& mac_address,
                      int rssi,
                      const std::string& strManufacturerData);
    void on_alive_message(const std::string &topic, const std::string &payload);
};

} // namespace esp32_ble_presense
} // namespace esphome
