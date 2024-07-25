# ESP32_BLE_presense
Tracker node ESP32 firmware for Home Assistant Format BLE Tracker integration https://github.com/formatBCE/Format-BLE-Tracker.

# Prerequisites:

0. Please make sure, that you're using high-quality ESP32 device. I had many troubles with some no-name devices. This will guarantee stable connection and scanning, as well as smooth setup/update.
1. ESP32 device (once more, use good one!)
2. 2.4 GHz WiFi network
3. MQTT server
4. ESPHome

# Usage
## TIP: Before this changes, custom header file was used. It's not needed now, you can delete it.

1. Create new device configuration in ESPHome for ESP32.

2. In main device config yaml, make changes according to [template](https://github.com/formatBCE/ESP32_BLE_presense/blob/main/esphome/esphome_node_template).

3. Install ESPHome firmware to ESP32 device.

4. Check the logs to see if it's reporting heartbeat, and add tracker devices in HA.

That's it, enjoy. If you're here, i believe, you know what to do with ESPHome.

[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/formatbce)
