# ESP32_BLE_presense
Tracker node ESP32 firmware for Home Assistant Format BLE Tracker integration https://github.com/formatBCE/Format-BLE-Tracker.

# Restrictions
1. This component works with framework `arduino`. So far there's no `esp-idf` support.
2. It is able to track following beacons:
  - Tile trackers
  - other simple BLE trackers
  - fitness trackers, that can advertise via BLE (tested on Amazfit Bip and Mi Bands)
  - Android phones with HA Companion app (via Proximity UUID from Bluetooth beacon)
  - ...?
3. It is definitely NOT able to track so far:
  - Apple devices, that require provisioning to send BLE data
4. Due to custom (and extensive) Bluetooth stack usage, other bluetooth functionality won't work on same ESP. Also I advise avoiding using it with other radio-heavy components.

# Prerequisites:

0. Please make sure, that you're using high-quality ESP32 device. I had many troubles with some no-name devices. This will guarantee stable connection and scanning, as well as smooth setup/update.
1. ESP32 device (once more, use good one!)
2. MQTT server
3. ESPHome

# Usage
## TIP: Before this changes, custom header file was used. It's not needed now, you can delete it.

1. Create new device configuration in ESPHome for ESP32.

2. In main device config yaml, make changes according to [template](https://github.com/formatBCE/ESP32_BLE_presense/blob/main/esphome/esphome_node_template).

3. Install ESPHome firmware to ESP32 device.

4. Check the logs to see if it's reporting heartbeat, and add tracker devices in HA.

That's it, enjoy. If you're here, i believe, you know what to do with ESPHome.

[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/formatbce)
