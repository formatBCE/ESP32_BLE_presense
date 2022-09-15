# ESP32_BLE_presense
Tracker node ESP32 firmware for Home Assistant Format BLE Tracker integration https://github.com/formatBCE/Format-BLE-Tracker.

# Prerequisites:

0. Please make sure, that you're using high-quality ESP32 device. I had many troubles with some no-name devices. This will guarantee stable connection and scanning, as well as smooth setup/update.
1. ESP32 device (once more, use good one!)
2. 2.4 GHz WiFi network
3. MQTT server
4. [OPTIONAL] ESPHome installed

# Usage

## [ESPHome](https://esphome.io/) (Recommended)
### At current stage, newest code and fixes are applied only for this variant. Custom firmware is deprecated.

* TIP 0: ESPHome variant does not create any device configuration or entities, because all configuration/updates goes through ESPHome.
* TIP 1: If you're updating from custom firmware, you may use ESPHome manual installation option (choose "Legacy" on export step) and update via OTA.
* TIP 2: You'll want to delete several retained messages in MQTT to clean old device config from Home Assistant:
  - check topic homeassistant/sensor/formatbce-ble-tracker-nodes
  - check topic formatbce-ble-tracker/trackers
  - check individual beacon topics under formatbce-ble-tracker/ for stale rooms subtopics
* TIP 3: If you will choose same name for new tracker, you won't need to change any logic in Home Assistant.

1. Create new device configuration in ESPHome for ESP32.

2. After initial configuration (you may skip connect/install steps), create file ble-node.h in config/esphome directory.

3. Paste contents of [ble-node.h](https://github.com/formatBCE/ESP32_BLE_presense/blob/main/esphome/ble-node.h) to created file.

4. In main device config yaml, make changes according to [template](https://github.com/formatBCE/ESP32_BLE_presense/blob/main/esphome/esphome_node_template).
(Pay attention to TODO lines).

5. Install ESPHome firmware to ESP32 device.

That's it, enjoy. If you're here, i believe, you know what to do with ESPHome.

## Custom firmware (deprecated, use ESPHome instead)

1. Get the code from repository, and flash it to ESP32 using PlatformIO or other IDE.
Or, download only bin file from latest release, and flash using ESP flasher or other flashing tool.
Use [this file](https://github.com/formatBCE/ESP32_BLE_presense/blob/main/partitions_singleapp.csv) for partitioning reference.
This is required only once, all other updates can be done over the air.

2. Place chip in preferred location and power it up. Make sure you use good power adapter! Device will start blinking fast with LED.

3. Find new WiFi network, named "EspBleScanner-(some numbers and letters)".
Connect to it from your PC/phone.
You should receive request to "authenticate" in the network, which will bring initial configuration screen to you.
If it didn't happen, open your browser and go to http://8.8.8.8 (or any domain, e.g. "https://google.com" - it will still redirect you).

4. Put your WiFi configuration and node name (basically, room name for device). 
This room name will be shown in Home Assistant sensor value, so pick wisely!
Use 2.4 GHz WiFi network.
If configuration is incorrect, you will be presented by error screen.
Otherwise, local device IP will be shown.

5. Connect to your network. Go to local device IP in your browser, and configure MQTT connection. Save config.

6. After saving configuration, ESP32 will restart, connect to WiFi and start scanning and sending data to MQTT. By default, there will be no tag data on MQTT server, if you didn't install Home Assistant integration https://github.com/formatBCE/Format-BLE-Tracker yet.

7. In Home Assistant you will be able to find new device with node current IP sensor, and current software version. Use it to examine/change device preferences, reset config or update firmware OTA.

# Troubleshooting (custom firmware)

1. If device is in AP mode (initial configuration needed), LED will be blinking 2 times per second.
2. If WiFi configuration is set up, but device could not connect to WiFi for any reasons, LED will be blinking once per second.
3. If WiFi is connected, but there's no MQTT configuration done, or MQTT server is inaccessible, LED will be blinking once per 2 seconds.
4. If everything is alright, LED will be constantly flashing. On event of finding any defined beacon, LED will shortly flash off. This way you can make sure, that data are sent to MQTT.

In case if you cannot find device IP address and go to configuration page (it may happen, for instance, when your WiFi network is down, or password was changed), you can manually reset device configuration. To do this, reboot device by button press or by reconnecting the power 5 times with period of less that 5 seconds. Device will erase configuration and return to Access Point mode.

# OTA (custom firmware)
- To avoid problems, use desktop for updating over the air. For me, updating with Android browser did not work.
- If during OTA update, progress bar is stuck, just relaunch process by hitting "Update" button again.


[!["Buy Me A Coffee"](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://www.buymeacoffee.com/formatbce)
