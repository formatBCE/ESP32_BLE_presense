#include <stdlib.h>
#include <WiFi.h>
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
String command = "";
std::vector<String> macs;

// Main fields
static const int scanTime = singleScanTime;
static const int waitTime = scanInterval;
String availabilityTopic;
String setTopic;
String telemetryTopic;

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
	StaticJsonDocument<256> tele;
	tele["ip"] = localIp;
	tele["config"] = command;
	//tele["hostname"] = WiFi.getHostname();
	//tele["scan_dur"] = scanTime;
	//tele["wait_dur"] = waitTime;
	char teleMessageBuffer[258];
	serializeJson(tele, teleMessageBuffer);

	if (mqttClient.publish(telemetryTopic.c_str(), 0, 0, teleMessageBuffer) == true) {
		Serial.println("Telemetry sent");
		return true;
	} else {
		Serial.println("Error sending telemetry");
		mqttClient.disconnect();
		return false;
	}
}

void sendDeviceState(String device, const char* state, int rssi) {
	if (mqttClient.connected()) {
		String normalized = device + "/";
	    normalized.replace(':', '_');
		String t_topic = esp_ble_presence_topic + normalized + tracker_topic + state_topic;
		String r_topic = esp_ble_presence_topic + normalized + node_name + rssi_topic + state_topic;
		if (mqttClient.publish(t_topic.c_str(), 0, 0, state) == true 
		 && mqttClient.publish(r_topic.c_str(), 0, 0, String(rssi).c_str()) == true) {
			Serial.println("Device data sent.");
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

void reportDevice(NimBLEAdvertisedDevice advertisedDevice) {
	String mac_address = advertisedDevice.getAddress().toString().c_str();
	mac_address.toUpperCase();
	if (std::find(macs.begin(), macs.end(), mac_address) == macs.end()) {
		return;
	}
	sendDeviceState(mac_address, payload_home_value, advertisedDevice.getRSSI());
}

void scanForDevices(void * parameter) {
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

void sendHaConfig(String device) {
	String normalized = device + "/";
	normalized.replace(':', '_');
	String unique = "esp_ble_tracker_" + normalized.substring(0, normalized.length() - 1);

	StaticJsonDocument<256> device_info;
	device_info[name_param] = device;
    StaticJsonDocument<128> idDoc;
    JsonArray idArray = idDoc.to<JsonArray>();
    idArray.add(unique);
	device_info[identifiers_param] = idArray;

	StaticJsonDocument<512> tracker_conf;
	tracker_conf[device_param] = device_info;
	tracker_conf[state_topic_param] = esp_ble_presence_topic + normalized + tracker_topic + state_topic;
	tracker_conf[name_param] = device + " tracker";
	tracker_conf[payload_home_param] = payload_home_value;
	tracker_conf[payload_not_home_param] = payload_not_home_value;
	tracker_conf[source_type_param] = source_type_value;
	tracker_conf[unique_id_param] = unique + "_tracker";
	char tracker_buffer[512];
	serializeJson(tracker_conf, tracker_buffer);

	StaticJsonDocument<512> rssi_conf;
	rssi_conf[device_param] = device_info;
	rssi_conf[state_topic_param] = esp_ble_presence_topic + normalized + node_name + rssi_topic + state_topic;
	rssi_conf[name_param] = device + " " + node_name + " RSSI";
	rssi_conf[unique_id_param] = unique + "_rssi_" + node_name;
	rssi_conf["state_class"] = "measurement";
	rssi_conf["unit_of_measurement"] = "dBm";
	rssi_conf["icon"] = "mdi:signal";
	char rssi_buffer[512];
	serializeJson(rssi_conf, rssi_buffer);

	if (
		mqttClient.publish((discovery_prefix + device_tracker_topic + normalized + tracker_topic + config_topic).c_str(), 0, true, tracker_buffer) == true
		&& mqttClient.publish((discovery_prefix + sensor_topic + normalized + node_name + rssi_topic + config_topic).c_str(), 0, true, rssi_buffer) == true) {
		Serial.println("Config sent for " + device);
	} else {
		Serial.println("Error sending HA config");
		mqttClient.disconnect();
	}
}

void readCommand() {
	preferences.begin(main_prefs, true);
	command = preferences.getString(macs_pref);
	preferences.end();
	DynamicJsonDocument doc(1024);
	deserializeJson(doc, command);
	JsonObject obj = doc.as<JsonObject>();
	JsonArray j_macs = obj["devices"];
	macs = std::vector<String>();
	for (JsonVariant v : j_macs) {
		String s = v.as<String>();
		if (s != "1") {
			s.toUpperCase();
        	macs.push_back(s);
			sendHaConfig(s);
		}
    }
}

void onMqttConnect(bool sessionPresent) {
  	Serial.println("Connected to MQTT.");
	digitalWrite(LED_BUILTIN, !LED_ON);
	mqttRetryAttempts = 0;
	
	if (mqttClient.publish(availabilityTopic.c_str(), 0, 1, "CONNECTED") == true) {
		Serial.print("Success sending message to topic:\t");
		Serial.println(availabilityTopic);
		mqttClient.subscribe(setTopic.c_str(), 2);
		sendTelemetry();
		readCommand();
	} else {
		Serial.println("Error sending message");
		mqttClient.disconnect();
	}
}

void saveCommand(String pld) {
	preferences.begin(main_prefs, false);
	preferences.putString(macs_pref, pld);
	preferences.end();
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
	Serial.println("MQTT command:");
	char pldArr[1024];
	strcpy(pldArr, payload);
	Serial.println(pldArr);
	saveCommand(pldArr);
	readCommand();
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
	if (var == "MQTT_TOPIC") {
		return esp_ble_presence_topic + node_name;
	}
	if (var == "MACS") {
		return command;
	}
	return String();
}

void notFound(AsyncWebServerRequest *request) {
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
	mqttClient.setWill(availabilityTopic.c_str(), 0, 1, "DISCONNECTED");
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
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send_P(200, "text/html", reset_html, processor);
	});
	server.on("/reset", HTTP_GET, [] (AsyncWebServerRequest *request) {
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
	Serial.print("Setting AP (Access Point)…");
  	WiFi.softAP(ap_ssid);
	IPAddress IP = WiFi.softAPIP();
  	Serial.print("AP IP address: ");
  	Serial.println(IP);
  	// Send web page with input fields to client
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send_P(200, "text/html", index_html, processor);
	});

	// Send a GET request to <ESP_IP>/get?input1=<inputMessage>
	server.on("/config", HTTP_GET, [] (AsyncWebServerRequest *request) {
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

	availabilityTopic = esp_ble_presence_topic + node_name + "/availability";
	setTopic = esp_ble_presence_topic + node_name + "/set";
	telemetryTopic = esp_ble_presence_topic + node_name + "/tele";

	return wifi_ssid != "" 
		&& mqtt_ip != "" 
		&& mqtt_port != 0 
		&& node_name != "";
}

void handleUpdate(AsyncWebServerRequest *request) {
  request->send(200, "text/html", update_html);
}

void handleDoUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
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
	server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){handleUpdate(request);});
	server.on("/doUpdate", HTTP_POST,
		[](AsyncWebServerRequest *request) {},
		[](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data,
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
