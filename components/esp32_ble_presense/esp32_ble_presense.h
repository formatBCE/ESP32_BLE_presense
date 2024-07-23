////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: AGPL-3.0-only
// (C) Copyright 2022-2024 - formatBCE

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mqtt/custom_mqtt_device.h"
#include "esphome/components/time/real_time_clock.h"

namespace ESP32_BLE_Presense {

class ESP32_BLE_Presense : public esphome::PollingComponent,
                         public esphome::mqtt::CustomMQTTDevice {
    std::string name;

    std::vector<std::string> macs;
    std::vector<std::string> uuids;

    esphome::time::RealTimeClock* rtc = 0;

public:

    ESP32_BLE_Presense();
    ~ESP32_BLE_Presense() = default;

    void update() override;
    void setup() override;

    void set_area(std::string area) {
        name = area;
    }

    void set_time(esphome::time::RealTimeClock* rtc) {
        this->rtc = rtc;
    }

    ESP32_BLE_Presense(const ESP32_BLE_Presense&) = delete;
    ESP32_BLE_Presense& operator=(const ESP32_BLE_Presense&) = delete;

private:

    friend class BleAdvertisedDeviceCallbacks;
    void reportDevice(const std::string& mac_address,
                      int rssi,
                      const std::string& strManufacturerData);
    void on_alive_message(const std::string &topic, const std::string &payload);
};

};
