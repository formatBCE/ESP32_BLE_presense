////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: AGPL-3.0-only
// (C) Copyright 2022-2024 - formatBCE

#include "esp32_ble_presense.h"

#include "esphome/core/log.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#define bleScanInterval 0x80 // Used to determine antenna sharing between Bluetooth and WiFi. Do not modify unless you are confident you know what you're doing
#define bleScanWindow 0x40 // Used to determine antenna sharing between Bluetooth and WiFi. Do not modify unless you are confident you know what you're doing

using namespace esphome;

namespace ESP32_BLE_Presense {

NimBLEScan* pBLEScan;

class BleAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

    ESP32_BLE_Presense& parent_;

public:
    BleAdvertisedDeviceCallbacks(ESP32_BLE_Presense& parent) : parent_(parent) {}

    void onResult(NimBLEAdvertisedDevice* device) {
        if (!device)
            return;

        parent_.reportDevice(device->getAddress().toString(),
                             device->getRSSI(),
                             device->getManufacturerData());
    }
};

static std::string capitalizeString(const std::string& s) {
    std::string ret;
    ret.reserve(s.size());

    std::transform(s.begin(), s.end(), std::back_inserter(ret),
                   [](unsigned char c){ return std::toupper(c); });
    return ret;
}

static unsigned long getTime() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return(0);
    }
    ::time(&now);
    return now;
}

ESP32_BLE_Presense::ESP32_BLE_Presense()
:  esphome::PollingComponent(5000) {
}

void ESP32_BLE_Presense::update() {
    if (!pBLEScan->isScanning()) {
        ESP_LOGD("format_ble", "Start scanning...");
        pBLEScan->start(0, nullptr, false);
    }
    ESP_LOGD("format_ble", "BLE scan heartbeat");
}

void ESP32_BLE_Presense::setup() {
    PollingComponent::setup();

    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new BleAdvertisedDeviceCallbacks(*this), true);
    pBLEScan->setInterval(bleScanInterval);
    pBLEScan->setWindow(bleScanWindow);
    pBLEScan->setActiveScan(false);
    pBLEScan->setMaxResults(0);
    configTime(0, 0, "pool.ntp.org");
    subscribe("format_ble_tracker/alive/+", &ESP32_BLE_Presense::on_alive_message);
}

void ESP32_BLE_Presense::reportDevice(const std::string& macAddress,
                                    int rssi,
                                    const std::string& manufacturerData) {

    std::string mac_address = capitalizeString(macAddress);
    unsigned long time = getTime();
    if (std::find(macs.begin(), macs.end(), mac_address) != macs.end()) {
        ESP_LOGD("format_ble", ("Sending for " + mac_address).c_str());
        publish_json("format_ble_tracker/" + mac_address + "/" + name, [=](JsonObject root) {
            root["rssi"] = rssi;
            root["timestamp"] = time;
        }, 1, true);
        return;
    }
    std::string strManufacturerData = manufacturerData;
    if (strManufacturerData != "") {
        uint8_t cManufacturerData[100];
        strManufacturerData.copy((char*)cManufacturerData, strManufacturerData.length(), 0);
        std::string uuid_str = capitalizeString(NimBLEUUID(cManufacturerData+4, 16, true).toString().c_str());
        if (std::find(uuids.begin(), uuids.end(), uuid_str) != uuids.end()) {
            ESP_LOGD("format_ble", ("Sending for " + uuid_str).c_str());
            publish_json("format_ble_tracker/" + uuid_str + "/" + name, [=](JsonObject root) {
                root["rssi"] = rssi;
                root["timestamp"] = time;
            }, 1, true);
            return;
        }
    }
}


void ESP32_BLE_Presense::on_alive_message(const std::string &topic, const std::string &payload) {
    std::string uid = capitalizeString(topic.substr(topic.find_last_of("/") + 1));
    if (payload == "True") {
        if (uid.rfind(":") != std::string::npos) {
            if (std::find(macs.begin(), macs.end(), uid) == macs.end()) {
                ESP_LOGD("format_ble", ("Adding MAC  " + uid).c_str());
                macs.push_back(uid);
            } else {
                ESP_LOGD("format_ble", ("Skipping duplicated MAC  " + uid).c_str());
            }
        } else if (uid.rfind("-") != std::string::npos) {
            if (std::find(uuids.begin(), uuids.end(), uid) == uuids.end()) {
                ESP_LOGD("format_ble", ("Adding UUID " + uid).c_str());
                uuids.push_back(uid);
            } else {
                ESP_LOGD("format_ble", ("Skipping duplicated UUID  " + uid).c_str());
            }
        }
        return;
    } else {
        ESP_LOGD("format_ble", ("Removing " + uid).c_str());
        macs.erase(std::remove(macs.begin(), macs.end(), uid), macs.end());
        uuids.erase(std::remove(uuids.begin(), uuids.end(), uid), uuids.end());
    }
}

}
