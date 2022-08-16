#include "esphome.h"
#include <stdlib.h>
#include "time.h"
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#define waitTime 1 // Define the interval in seconds between scans
#define scanTime 9 // Define the duration of a single scan in seconds
#define bleScanInterval 0x80 // Used to determine antenna sharing between Bluetooth and WiFi. Do not modify unless you are confident you know what you're doing
#define bleScanWindow 0x10 // Used to determine antenna sharing between Bluetooth and WiFi. Do not modify unless you are confident you know what you're doing

unsigned long lastBleScan = 0;
NimBLEScan* pBLEScan;
TaskHandle_t nimBLEScan;

void scanForDevices(void* parameter) {
        while (1) {
            if ((millis() - lastBleScan > (waitTime * 1000) || lastBleScan == 0)) {
                ESP_LOGD("format_ble", "Scanning...\t");
                pBLEScan->start(scanTime);
                ESP_LOGD("format_ble", "Scanning done.");
                pBLEScan->clearResults();
                lastBleScan = millis();
            }
        }
    }

class BleNodeComponent : public Component, public CustomMQTTDevice {

    std::vector<std::string> macs;
    std::vector<std::string> uuids;

    unsigned long getTime() {
        time_t now;
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            return(0);
        }
        time(&now);
        return now;
    }

    void reportDevice(NimBLEAdvertisedDevice& advertisedDevice) {
        std::string mac_address = capitalizeString(advertisedDevice.getAddress().toString().c_str());
        int rssi = advertisedDevice.getRSSI();
        unsigned long time = getTime();
        if (std::find(macs.begin(), macs.end(), mac_address) != macs.end()) {
            ESP_LOGD("format_ble", ("Sending for " + mac_address).c_str());
            publish_json("format_ble_tracker/" + mac_address + "/" + name, [=](JsonObject root) {
                root["rssi"] = rssi;
                root["timestamp"] = time;
            }, 1, true);
            return;
        }
        std::string strManufacturerData = advertisedDevice.getManufacturerData();
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

    std::string capitalizeString(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c){ return std::toupper(c); });
        return s;
    }

    class BleAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

        BleNodeComponent& parent_;

        public:
            BleAdvertisedDeviceCallbacks(BleNodeComponent& parent) : parent_(parent) {}

            void onResult(NimBLEAdvertisedDevice* device) {
                parent_.reportDevice(*device);
            }
    };
    
 public:
    std::string name;
    BleNodeComponent(std::string given_name) {
        name = given_name;
    }

    void setup() override {
        NimBLEDevice::init("");
        NimBLEDevice::setPower(ESP_PWR_LVL_P9);
        pBLEScan = NimBLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new BleAdvertisedDeviceCallbacks(*this));
        pBLEScan->setInterval(bleScanInterval);
        pBLEScan->setWindow(bleScanWindow);
        xTaskCreatePinnedToCore(scanForDevices, "BLE Scan", 4096, pBLEScan, 1, &nimBLEScan, 1);
        configTime(0, 0, "pool.ntp.org");
        subscribe("format_ble_tracker/alive/+", &BleNodeComponent::on_alive_message);
    }

    void on_alive_message(const std::string &topic, const std::string &payload) {
        std::string uid = topic.substr(topic.find_last_of("/") + 1);
        if (payload == "True") {
            if (uid.find_last_of(":") >= 0) {
                if (std::find(macs.begin(), macs.end(), uid) == macs.end()) {
                    ESP_LOGD("format_ble", ("Adding MAC  " + uid).c_str());
                    macs.push_back(uid);
                } else {
                    ESP_LOGD("format_ble", ("Skipping duplicated MAC  " + uid).c_str());
                }
            } else if (uid.find_last_of("-") >= 0) {
                if (std::find(uuids.begin(), uuids.end(), uid) == uuids.end()) {
                    ESP_LOGD("format_ble", ("Adding UUID" + uid).c_str());
                    uuids.push_back(uid);
                } else {
                    ESP_LOGD("format_ble", ("Skipping duplicated UUID  " + uid).c_str());
                }
            }
            return;
            publish("the/other/topic", "Hello World!");
        } else {
            ESP_LOGD("format_ble", ("Removing " + uid).c_str());
            macs.erase(std::remove(macs.begin(), macs.end(), uid), macs.end());
            uuids.erase(std::remove(uuids.begin(), uuids.end(), uid), uuids.end());
        }
    }
};
