# ESP32_BLE_presense
Tracker node ESP32 firmware for Home Assistant Format BLE Tracker integration https://github.com/formatBCE/Format-BLE-Tracker.

# Prerequisites:

1. ESP32 device
2. 2.4 GHz WiFi network
3. MQTT server

# Usage

1. Get the code from repository, and flash it to ESP32 using PlatformIO or other IDE.
Or, download only firmware.bin file, and flash using ESP flasher or other flashing tool.
Use [this file](https://github.com/formatBCE/ESP32_BLE_presense/blob/main/partitions_singleapp.csv) for partitioning reference.

2. Place chip in preferred location and power it up. Make sure you use good power adapter!

3. Find new WiFi network, named "EspBleScanner".
Connect to it from your PC/phone, and go to http://192.168.4.1 in your browser.

4. Configure node with WiFi and MQTT parameters, and assign room name. This room name will be shown in Home Assistant sensor value, so pick wisely!
Make sure you did type everything correctly before saving configuration.
Use 2.4 GHz WiFi network.

5. After saving configuration, ESP32 will restart, connect to WiFi and start scanning and sending data to MQTT. By default, there will be no tag data on MQTT server, if you didn't install Home Assistant integration https://github.com/formatBCE/Format-BLE-Tracker yet.
6. In Home Assistant you will be able to find new device with node current IP sensor. Use it to examine node preferences, reset config or update firmware OTA.


