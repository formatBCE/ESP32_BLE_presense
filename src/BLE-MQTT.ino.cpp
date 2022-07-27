#include <stdlib.h>
#include <WiFi.h>
#include "time.h"
extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

#include <AsyncTCP.h>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <Preferences.h>

#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEEddystoneURL.h"
#include "NimBLEEddystoneTLM.h"
#include "NimBLEBeacon.h"

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>
#include "Settings.h"

// Config fields
AsyncWebServer server(80);
size_t content_len;
Preferences preferences;
bool isSetUp = false;

String wifi_ssid = "";
String wifi_pwd = "";
String mqtt_ip = "";
int mqtt_port = 0;
String mqtt_user = "";
String mqtt_pass = "";
String node_name = "";
std::vector<String> macs;
std::vector<String> uuids;

// Main fields
static const int scanTime = singleScanTime;
static const int waitTime = scanInterval;
String availabilityTopic;
String stateTopic;

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
String localIp;
byte mqttRetryAttempts = 0;
byte wifiRetryAttempts = 0;
unsigned long last = 0;
NimBLEScan* pBLEScan;
TaskHandle_t NimBLEScan;
size_t commandLength;

void connectToWifi() {
  	Serial.println("Connecting to WiFi...");
	WiFi.begin(wifi_ssid.c_str(), wifi_pwd.c_str());
	WiFi.setHostname(node_name.c_str());
}

bool handleWifiDisconnect() {
	if (WiFi.isConnected()) {
		Serial.println("WiFi appears to be connected. Not retrying.");
		return true;
	}
	digitalWrite(LED_BUILTIN, !LED_ON);
	delay(500);
	digitalWrite(LED_BUILTIN, LED_ON);
	if (wifiRetryAttempts > 10) {
		Serial.println("Too many retries. Restarting.");
		ESP.restart();
	} else {
		wifiRetryAttempts++;
	}
	if (mqttClient.connected()) {
		mqttClient.disconnect();
	}
	if (xTimerIsTimerActive(mqttReconnectTimer) != pdFALSE) {
		xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
	}

	if (xTimerReset(wifiReconnectTimer, 0) == pdFAIL) {
		Serial.println("failed to restart");
		xTimerStart(wifiReconnectTimer, 0);
		return false;
	} else {
		Serial.println("restarted");
		return true;
	}
}

void connectToMqtt() {
  	Serial.println("Connecting to MQTT");
	if (WiFi.isConnected()) {
		mqttClient.setCredentials(mqtt_user.c_str(), mqtt_pass.c_str());
		mqttClient.setClientId(node_name.c_str());
	 	mqttClient.connect();
	} else {
		Serial.println("Cannot reconnect MQTT - WiFi error");
		handleWifiDisconnect();
	}
}

void handleMqttDisconnect() {
	if (mqttRetryAttempts > 10) {
		Serial.println("Too many retries. Restarting.");
		ESP.restart();
	} else {
		mqttRetryAttempts++;
	}
	if (WiFi.isConnected()) {
		Serial.println("Starting MQTT reconnect timer");
		if (xTimerReset(mqttReconnectTimer, 0) == pdFAIL) {
			Serial.println("failed to restart");
			xTimerStart(mqttReconnectTimer, 0);
		} else {
			Serial.println("restarted");
		}
  	} else {
		Serial.print("Disconnected from WiFi; starting WiFi reconnect timiler\t");
		handleWifiDisconnect();
	}
}

void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %x\n\r", event);

	switch(event) {
	case SYSTEM_EVENT_STA_GOT_IP:
		Serial.print("IP address: \t");
		Serial.println(WiFi.localIP());
		localIp = WiFi.localIP().toString().c_str();
		Serial.print("Hostname: \t");
		Serial.println(WiFi.getHostname());
		configTime(0, 0, ntp_server);
		connectToMqtt();
		if (xTimerIsTimerActive(wifiReconnectTimer) != pdFALSE) {
			Serial.println("Stopping wifi reconnect timer");
			xTimerStop(wifiReconnectTimer, 0);
		}
		wifiRetryAttempts = 0;
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		Serial.println("WiFi lost connection, resetting timer\t");
		handleWifiDisconnect();
		break;
	case SYSTEM_EVENT_WIFI_READY:
		Serial.println("Wifi Ready");
		handleWifiDisconnect();
		break;
	case SYSTEM_EVENT_STA_START:
		Serial.println("STA Start");
		tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, node_name.c_str());
		if (xTimerIsTimerActive(wifiReconnectTimer) != pdFALSE) {
			TickType_t xRemainingTime = xTimerGetExpiryTime( wifiReconnectTimer ) - xTaskGetTickCount();
			Serial.print("WiFi Time remaining: ");
			Serial.println(xRemainingTime);
		} else {
			Serial.println("WiFi Timer is inactive; resetting\t");
			handleWifiDisconnect();
		}
		break;
	case SYSTEM_EVENT_STA_STOP:
		Serial.println("STA Stop");
		handleWifiDisconnect();
		break;
    }
}

bool sendTelemetry() {
	if (mqttClient.publish(stateTopic.c_str(), 0, false, localIp.c_str()) == true) {
		Serial.println("State sent");
		return true;
	} else {
		Serial.println("Error sending telemetry");
		mqttClient.disconnect();
		return false;
	}
}

unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}

void sendDeviceState(String device, int rssi) {
	if (mqttClient.connected()) {
		String topic = root_topic + device + "/" + node_name;
		StaticJsonDocument<512> pld;
		pld["rssi"] = rssi;
		pld["timestamp"] = getTime();
		char pld_buffer[128];
		serializeJson(pld, pld_buffer);
		if (mqttClient.publish(topic.c_str(), 0, true, pld_buffer, strlen(pld_buffer)) == true) {
			Serial.println("Sent data for " + device);
		} else {
			Serial.print("Error sending device data message.");
			mqttClient.disconnect();
		}
	} else {
		Serial.println("MQTT disconnected.");
		if (xTimerIsTimerActive(mqttReconnectTimer) != pdFALSE) {
			TickType_t xRemainingTime = xTimerGetExpiryTime( mqttReconnectTimer ) - xTaskGetTickCount();
			Serial.print("Time remaining: ");
			Serial.println(xRemainingTime);
		} else {
			handleMqttDisconnect();
		}
	}
}

void reportDevice(NimBLEAdvertisedDevice& advertisedDevice) {
	String mac_address = advertisedDevice.getAddress().toString().c_str();
	mac_address.toUpperCase();
	if (std::find(macs.begin(), macs.end(), mac_address) != macs.end()) {
		sendDeviceState(mac_address, advertisedDevice.getRSSI());
		return;
	}
	std::string strManufacturerData = advertisedDevice.getManufacturerData();
	if (strManufacturerData != "") {
		uint8_t cManufacturerData[100];
		strManufacturerData.copy((char*)cManufacturerData, strManufacturerData.length(), 0);
		String uuid_str = NimBLEUUID(cManufacturerData+4, 16, true).toString().c_str();
		uuid_str.toUpperCase();
		if (std::find(uuids.begin(), uuids.end(), uuid_str) != uuids.end()) {
			sendDeviceState(uuid_str, advertisedDevice.getRSSI());
			return;
		}
	}
}

void scanForDevices(void* parameter) {
	while (1) {
		if (WiFi.isConnected() && (millis() - last > (waitTime * 1000) || last == 0)) {
			Serial.println("Scanning...\t");
			pBLEScan->start(scanTime);
			Serial.println("Scanning done.");
			pBLEScan->clearResults();
			sendTelemetry();
			last = millis();
		}
	}
}

class BleAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* device) {
		if (mqttClient.connected()) {
			reportDevice(*device);
		} else {
			Serial.println("Cannot report: mqtt disconnected");
			if (xTimerIsTimerActive(mqttReconnectTimer) != pdFALSE) {
				TickType_t xRemainingTime = xTimerGetExpiryTime( mqttReconnectTimer ) - xTaskGetTickCount();
				Serial.print("Time remaining: ");
				Serial.println(xRemainingTime);
			} else {
				handleMqttDisconnect();
			}
		}
    }
};

void sendHaConfig() {
	String prefix = "format_ble_tracker_rooms";

	StaticJsonDocument<256> device_info;
	device_info["name"] = "Format BLE Tracker - " + node_name;
	device_info["configuration_url"] = "http://" + localIp;
	device_info["sw_version"] = String(version);
    StaticJsonDocument<128> idDoc;
    JsonArray idArray = idDoc.to<JsonArray>();
    idArray.add(prefix + "_" + node_name);
	device_info["identifiers"] = idArray;

	StaticJsonDocument<512> tracker_conf;
	tracker_conf["device"] = device_info;
	tracker_conf["state_topic"] = stateTopic;
	tracker_conf["availability_topic"] = availabilityTopic;
	tracker_conf["name"] = node_name + " IP address";
	tracker_conf["unique_id"] = prefix + "_" + node_name + "_ip";
	char tracker_buffer[512];
	serializeJson(tracker_conf, tracker_buffer);

	if (
		mqttClient.publish((discovery_prefix + sensor_topic  + prefix + "/" + node_name + "/" + config_topic).c_str(), 0, true, tracker_buffer, strlen(tracker_buffer)) == true) {
		Serial.println("Config sent for " + node_name);
	} else {
		Serial.println("Error sending HA config");
		mqttClient.disconnect();
	}
}

void onMqttConnect(bool sessionPresent) {
  	Serial.println("Connected to MQTT.");
	digitalWrite(LED_BUILTIN, !LED_ON);
	mqttRetryAttempts = 0;
	
	if (mqttClient.publish(availabilityTopic.c_str(), 0, true, "online") == true) {
		Serial.print("Success sending message to topic:\t");
		Serial.println(availabilityTopic);
		String alive_topic = root_topic + "alive/+";
		mqttClient.subscribe(alive_topic.c_str(), 2);
		sendHaConfig();
		sendTelemetry();
	} else {
		Serial.println("Error sending message");
		mqttClient.disconnect();
	}
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
	char topicArr[256];
	strcpy(topicArr, topic);
	String tStr = topicArr;
	String uid = tStr.substring(tStr.lastIndexOf("/") + 1);
	if (payload) {
		char pldArr[32];
		strcpy(pldArr, payload);
		String pld = pldArr;
		if (pld.indexOf("True") >= 0) {
			if (uid.indexOf(":") >= 0) {
				if (std::find(macs.begin(), macs.end(), uid) == macs.end()) {
					Serial.println("Adding MAC  " + uid);
					macs.push_back(uid);
				} else {
					Serial.println("Skipping duplicated MAC  " + uid);
				}
			} else if (uid.indexOf("-") >= 0) {
				if (std::find(uuids.begin(), uuids.end(), uid) == uuids.end()) {
					Serial.println("Adding UUID" + uid);
					uuids.push_back(uid);
				} else {
					Serial.println("Skipping duplicated UUID  " + uid);
				}
			}
			return;
		}
	}
	Serial.println("Removing " + uid);
	macs.erase(std::remove(macs.begin(), macs.end(), uid), macs.end());
	uuids.erase(std::remove(uuids.begin(), uuids.end(), uid), uuids.end());
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
	Serial.println("Disconnected from MQTT.");
	digitalWrite(LED_BUILTIN, LED_ON);
	handleMqttDisconnect();
}

String processor(const String& var) {
	if (var == "VERSION") {
		return String(version);
	}
	if (var == "WIFI") {
		return wifi_ssid;
	}
	if (var == "MQTT_IP") {
		return mqtt_ip;
	}
	if (var == "MQTT_PORT") {
		return String(mqtt_port);
	}
	if (var == "MQTT_USER") {
		return mqtt_user;
	}
	if (var == "ROOM_NAME") {
		return node_name;
	}
	return String();
}

void notFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "Not found");
}

void mainSetup() {
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LED_ON);

	mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
	wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

	WiFi.onEvent(WiFiEvent);

	mqttClient.onConnect(onMqttConnect);
	mqttClient.onDisconnect(onMqttDisconnect);
	mqttClient.onMessage(onMqttMessage);

	mqttClient.setServer(mqtt_ip.c_str(), mqtt_port);
	mqttClient.setWill(availabilityTopic.c_str(), 0, 1, "offline");
	mqttClient.setKeepAlive(60);

	connectToWifi();

  	NimBLEDevice::init("");
	NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  	pBLEScan = NimBLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new BleAdvertisedDeviceCallbacks());
	pBLEScan->setInterval(bleScanInterval);
	pBLEScan->setWindow(bleScanWindow);
	xTaskCreatePinnedToCore(
		scanForDevices,
		"BLE Scan",
		4096,
		pBLEScan,
		1,
		&NimBLEScan,
		1);
	server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
		request->send_P(200, "text/html", reset_html, processor);
	});
	server.on("/reset", HTTP_GET, [] (AsyncWebServerRequest* request) {
		preferences.begin(main_prefs, false);
		preferences.clear();
		preferences.end();
		request->send_P(200, "text/html", confirm_html, processor);
		digitalWrite(LED_BUILTIN, LED_ON);
		delay(3000);
		ESP.restart();
	});
}

void mainLoop() {
	TIMERG0.wdt_wprotect=TIMG_WDT_WKEY_VALUE;
	TIMERG0.wdt_feed=1;
	TIMERG0.wdt_wprotect=0;
}

void configSetup() {
	byte mac[6];
	WiFi.macAddress(mac);
  	String ssid = ap_ssid + "-" + String(mac[4],HEX) + String(mac[5],HEX);
	Serial.println("Setting AP " + ssid);
	WiFi.softAP(ssid.c_str());
	IPAddress IP = WiFi.softAPIP();
  	Serial.print("AP IP address: ");
  	Serial.println(IP);
  	// Send web page with input fields to client
	server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){
		request->send_P(200, "text/html", index_html, processor);
	});

	// Send a GET request to <ESP_IP>/get?input1=<inputMessage>
	server.on("/config", HTTP_GET, [] (AsyncWebServerRequest* request) {
		String wifi_ssid_param = request->getParam(PARAM_INPUT_1)->value();
		String wifi_pwd_param = request->getParam(PARAM_INPUT_2)->value();
		String mqtt_ip_param = request->getParam(PARAM_INPUT_3)->value();
		String mqtt_port_param = request->getParam(PARAM_INPUT_4)->value();
		String mqtt_user_param = request->getParam(PARAM_INPUT_5)->value();
		String mqtt_pass_param = request->getParam(PARAM_INPUT_6)->value();
		String node_name_param = request->getParam(PARAM_INPUT_7)->value();
		preferences.begin(main_prefs, false);
		preferences.clear();
		preferences.putString(wifi_ssid_pref, wifi_ssid_param);
		preferences.putString(wifi_pwd_pref, wifi_pwd_param);
		preferences.putString(mqtt_ip_pref, mqtt_ip_param);
		preferences.putInt(mqtt_port_pref, mqtt_port_param.toInt());
		preferences.putString(mqtt_user_pref, mqtt_user_param);
		preferences.putString(mqtt_pass_pref, mqtt_pass_param);
		preferences.putString(node_name_pref, node_name_param);
		preferences.end();

		request->send_P(200, "text/html", confirm_html, processor);
		delay(3000);
		ESP.restart();
	});
}

bool readPrefs() {
	preferences.begin(main_prefs, true);
	wifi_ssid = preferences.getString(wifi_ssid_pref);
	wifi_pwd = preferences.getString(wifi_pwd_pref);
	mqtt_ip = preferences.getString(mqtt_ip_pref);
	mqtt_port = preferences.getInt(mqtt_port_pref);
	mqtt_user = preferences.getString(mqtt_user_pref);
	mqtt_pass = preferences.getString(mqtt_pass_pref);
	node_name = preferences.getString(node_name_pref);
	preferences.end();

	availabilityTopic = root_topic + tracker_topic + node_name + "/availability";
	stateTopic = root_topic + tracker_topic + node_name + "/state";

	return wifi_ssid != "" 
		&& mqtt_ip != "" 
		&& mqtt_port != 0 
		&& node_name != "";
}

void handleUpdate(AsyncWebServerRequest* request) {
  request->send(200, "text/html", update_html);
}

void handleDoUpdate(AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
	if (!index){
		Serial.println("Update");
		content_len = request->contentLength();
		if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
			Update.printError(Serial);
		}
	}

	if (Update.write(data, len) != len) {
		Update.printError(Serial);
	}

	if (final) {
		if (!Update.end(true)){
			Update.printError(Serial);
		} else {
			Serial.println("Update complete");
			Serial.flush();
			ESP.restart();
		}
	}
}

void printProgress(size_t prg, size_t sz) {
  	Serial.printf("Progress: %d%%\n", (prg*100)/content_len);
}

void commonSetup() {
	server.on("/update", HTTP_GET, [](AsyncWebServerRequest* request){handleUpdate(request);});
	server.on("/doUpdate", HTTP_POST,
		[](AsyncWebServerRequest* request) {},
		[](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data,
					size_t len, bool final) {
							handleDoUpdate(request, filename, index, data, len, final);
						}
	);
	server.onNotFound(notFound);
	server.begin();
	Update.onProgress(printProgress);
}

void setup() {
	Serial.begin(115200);
	isSetUp = readPrefs();
	if (isSetUp) {
	  	mainSetup();
	} else {
		configSetup();
	}
	commonSetup();
}

void loop() {
	if (isSetUp) {
		mainLoop();
	}
}
