//#define ESP8266


#if defined ESP8266
#include <ESP8266WiFi.h>          
#else
#include <WiFi.h>          
#endif

//needed for library
#include <DNSServer.h>
#if defined ESP8266
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif
#include <WiFiManager.h>      

#include <PubSubClient.h>
#include "stdafx.h"

#include "ObjectClass.h"
#include "OnewireSensor.h"
#include "SensorFactory.h"
#include "TemperatureSensor.h"
#include "DoorSensor.h"
#include "HornSensor.h"
#ifdef ESP8266
#include "TFTDisplay.h"
#endif
#include "ESP8266Webduino.h"
#include "MQTTClientClass.h"
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "Logger.h"
#include "Shield.h"
#include "Command.h"
#include <Time.h>
#include "TimeLib.h"
#include "POSTData.h"
#include <Wire.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include "Ticker.h"
#ifdef ESP8266
#include <ESP8266httpUpdate.h>
#else
#include <ESP32httpUpdate.h>
#endif

#ifdef ESP8266
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#endif

#include "SoftwareSerial.h"
//SoftwareSerial swSer1(D1, D2, false, 256);
//extern SoftwareSerial* swSer1;
//extern SoftwareSerial swSer1(D1, D2, false, 256);

//extern SoftwareSerial *swSer1;
//extern SoftwareSerial nextionSoftwareSerial;

#include "NextionDisplay.h"
//#include "Nextion.h"
NextionDisplay nextDisplay;





//void triggerUpdateTime();

/*Ticker updatetimeTimer(triggerUpdateTime, 30000); // once, immediately
bool timeNotUpdated = true;

void triggerUpdateTime() {
	logger.println(tag, F("\n\t >triggerUpdateTime"));
	timeNotUpdated = true;
}*/
extern void resetWiFiManagerSettings();
extern bool mqtt_publish(String topic, String message);
extern void reboot(String reason);


//bool checkHTTPUpdate = true; //true;
//bool mqttLoaded = false; //true;
bool shouldSaveConfig = true;
WiFiManager wifiManager;
//ESPDisplay espDisplay;
Logger logger;
String tag = "Webduino";
Shield shield;
MQTTClientClass mqttclient;

//WiFiServer server(80);
WiFiClient client;

//unsigned long lastCommandFailed = 0;
//const int commandFailed_interval = 60000 * 30;// *12;// 60 secondi * 15 minuti
unsigned long lastReboot = 0;
const int reboot_interval = 3600000 * 24;// 24 ore





void resetWiFiManagerSettings() {
	logger.print(tag, F("\n\n\t >>resetWiFiManagerSettings"));
	wifiManager.resetSettings();
	logger.println(tag, F("\n\t <<resetWiFiManagerSettings"));
}

void reboot(String reason) {
	shield.setRebootReason(reason);
	shield.writeRebootReason();
	ESP.restart();
}

void saveConfigCallback() {
	logger.println(tag, F("Should save config"));
	shouldSaveConfig = true;
}

bool testWifi() {

	// The extra parameters to be configured (can be either global or just in the setup)
	// After connecting, parameter.getValue() will get you the configured value
	// id/name placeholder/prompt default length
	String server = shield.getServerName();
	String serverport = String(shield.getServerPort());
	String mqttserver = shield.getMQTTServer();
	String mqttport = String(shield.getMQTTPort());

	WiFiManagerParameter custom_server("server", "server", server.c_str(), 40);
	WiFiManagerParameter custom_server_port("port", "port", serverport.c_str(), 5);
	WiFiManagerParameter custom_mqtt_server("mqttserver", "mqtt server", mqttserver.c_str(), 40);
	WiFiManagerParameter custom_mqtt_port("mqttport", "mqtt port", mqttport.c_str(), 5);

	// put your setup code here, to run once:
	//WiFiManager wifiManager;

	//set config save notify callback
	//wifiManager.setSaveConfigCallback(saveConfigCallback);

	// Uncomment and run it once, if you want to erase all the stored information
	//wifiManager.resetSettings();
	if (shield.getResetSettings()) {
		wifiManager.resetSettings();
		shield.setResetSettings(false);

	}

	//add all your parameters here
	wifiManager.addParameter(&custom_server);
	wifiManager.addParameter(&custom_server_port);
	wifiManager.addParameter(&custom_mqtt_server);
	wifiManager.addParameter(&custom_mqtt_port);

	WiFiManagerParameter custom_text("<p>This is just a text paragraph</p>");
	wifiManager.addParameter(&custom_text);
	//wifiManager.addParameter(&custom_blynk_token);

	wifiManager.setConfigPortalTimeout(180);

	//first parameter is name of access point, second is the password
	//wifiManager.autoConnect("AP-NAME", "passwd");

	//wifiManager.autoConnect("AP-NAME");
	//or if you want to use and auto generated name from 'ESP' and the esp's Chip ID use
	//wifiManager.autoConnect();
	if (!wifiManager.autoConnect()) {
		shield.setEvent(F("failed to connect and hit timeout"));
		shield.invalidateDisplay();
		logger.println(tag, F("failed to connect and hit timeout"));
		delay(3000);
		//ESP.restart();
		reboot("Autoconnect Timeout");

	}

	//if you get here you have connected to the WiFi
	logger.println(tag, F("connected..."));
	shield.setEvent(F("Wifi connected"));
	shield.invalidateDisplay();

	if (shouldSaveConfig) {

		shield.setServerName(custom_server.getValue());
		int serverport = atoi(custom_server_port.getValue());
		shield.setServerPort(serverport);
		shield.setMQTTServer(custom_mqtt_server.getValue());
		int mqttport = atoi(custom_mqtt_port.getValue());
		shield.setMQTTPort(mqttport);

		shield.setResetSettings(false);

		shield.writeConfig();
	}

	//read updated parameters
	Serial.println(custom_server.getValue());
	Serial.println(custom_server_port.getValue());
	Serial.println(custom_mqtt_server.getValue());
	Serial.println(custom_mqtt_port.getValue());

	return true;
}

void messageReceived(char* topic, byte* payload, unsigned int length) {

	logger.println(tag, F(">>messageReceived"));
	logger.print(tag, F("\n\t topic="));
	logger.print(tag, String(topic));

#ifdef ESP8266
	ESP.wdtFeed();
#endif // ESP8266

	String message = "";
	for (int i = 0; i < length; i++) {
		message += char(payload[i]);
	}
	shield.parseMessageReceived(String(topic), message);
	logger.println(tag, F("<<messageReceived"));
}

bool reconnect() {
	logger.print(tag, F("\n\n\t >>reconnect"));
	shield.setStatus(F("CONNECTING.."));
	// Loop until we're reconnected
	String clientId = "ESP8266Client" + shield.getMACAddress();
	int i = 0;
	while (!mqttclient.connected() && i < 3) {
#ifdef ESP8266
		ESP.wdtFeed();
#endif // ESP8266
		logger.print(tag, F("\n\t Attempting MQTT connection..."));
		logger.print(tag, F("\n\t temptative "));
		logger.print(tag, String(i));
		// Attempt to connect			
		if (mqttclient.connect(clientId)) {
			logger.print(tag, F("\n\t connected"));
			// Once connected, publish an announcement...
			//mqttclient.publish("send"/*topic.c_str()*/, "hello world");
			String topic = "fromServer/shield/" + shield.getMACAddress() + "/#";
			logger.print(tag, F("\n\t Subscribe to topic:"));
			logger.print(tag, topic);
			mqttclient.subscribe(topic.c_str());
			shield.setStatus(F("ONLINE"));
			logger.print(tag, F("\n\t <<reconnect\n\n"));
			return true;
		}
		else {
			logger.print(tag, F("\n\tfailed, rc="));
			logger.print(tag, mqttclient.state());
			logger.print(tag, F("\n\ttry again in 1 seconds"));
			// Wait 1 seconds before retrying
			delay(1000);
			i++;
		}
	}

	shield.setStatus(F("OFFLINE"));
	logger.print(tag, F("\n\t<<reconnect FAILED\n\n"));
	return false;
}

void setup()
{
	//Serial.begin(9600);
	Serial.begin(115200);
	delay(10);


	Logger::init();
	logger.print(tag, F("\n\t >>setup"));
	logger.print(tag, F("\n\n *******************RESTARTING************************"));

	shield.init();

	nextDisplay.init();
	//swSer1.begin(9600);
	//nexInit();
	//t0.setText("State: xxxxx");
	//b0.attachPop(bOnPopCallback, &b0);


	shield.setEvent(F("read config.."));
	shield.invalidateDisplay();
	shield.readConfig();
	shield.readRebootReason();
#ifdef ESP8266
	shield.drawString(0, 0, F("restarting.."), 1, ST7735_WHITE);
#endif
	shield.setEvent("restarting...ver" + shield.swVersion);
	shield.invalidateDisplay();
	logger.print(tag, F("\n\t Versione="));
	logger.print(tag, shield.swVersion);
		
	// Connect to WiFi network
	if (testWifi()) {

		shield.setEvent(F("connecting wifi.."));
		shield.invalidateDisplay();

		checkForSWUpdate();

#ifdef ESP8266
		ESP.wdtDisable();
		ESP.wdtFeed();
#endif
		shield.localIP = WiFi.localIP().toString();
		logger.print(tag, shield.localIP);

		shield.setEvent(F("Init MQTT"));
		/*mqttLoaded = */initMQTTServer();
	}

	lastReboot = millis();
	shield.lastCheckHealth = millis();

	logger.println(tag, F("\n\t<<setup\n\n"));
}

bool mqtt_publish(String topic, String message) {

	logger.print(tag, F("\n\t >>mqtt_publish"));
	logger.print(tag, F("\n\t topic: "));
	logger.print(tag, topic);
	logger.print(tag, F("\n\t message: "));
	logger.print(tag, message);

#ifdef ESP8266
	ESP.wdtFeed();
#endif // ESP8266

	bool res = false;
	if (!client.connected()) {
		logger.print(tag, F("\n\t OFFLINE - payload NOT sent!!!\n"));
	}
	else {
		res = mqttclient.publish(topic.c_str(), message.c_str());
		if (!res) {
			logger.print(tag, F("\n\t lastcommandfailed"));
			//lastCommandFailed = millis();
		}
	}
	logger.print(tag, F("\n\t <<mqtt_publish res="));
	logger.print(tag, Logger::boolToString(res));
	return res;
}

void checkForSWUpdate() {

#ifdef ESP8266

	logger.println(tag, F(">>checkForSWUpdate"));
	//delay(2000);

	//t_httpUpdate_return ret = ESPhttpUpdate.update("http://192.168.1.3:8080//webduino/ota",Shield::swVersion);
	String updatePath = "http://" + shield.getServerName() + ":" + shield.getServerPort() + "//webduino/ota";
	logger.print(tag, "\n\t check for sw update " + updatePath);
	logger.print(tag, "\n\t current version " + shield.swVersion);
	t_httpUpdate_return ret = ESPhttpUpdate.update(updatePath, shield.swVersion);

	switch (ret) {
	case HTTP_UPDATE_FAILED:

		logger.print(tag, F("\n\t HTTP_UPDATE_FAILD Error "));
		logger.print(tag, String(ESPhttpUpdate.getLastError()));
		logger.print(tag, F(" "));
		logger.print(tag, ESPhttpUpdate.getLastErrorString().c_str());

		shield.setEvent(F("sw Update failed"));
		break;

	case HTTP_UPDATE_NO_UPDATES:
		logger.print(tag, F("\n\t HTTP_UPDATE_NO_UPDATES"));
		shield.setEvent(F("no sw update available"));
		break;

	case HTTP_UPDATE_OK:
		logger.print(tag, F("\n\t HTTP_UPDATE_OK"));
		shield.setEvent(F("sw updated"));
		break;
	}
	logger.println(tag, F("<<checkForSWUpdate"));

#endif
}

bool initMQTTServer() {
	logger.println(tag, F("\n\t >>initMQTTServer"));
	mqttclient.init(&client);
	mqttclient.setServer(shield.getMQTTServer(), shield.getMQTTPort());
	mqttclient.setCallback(messageReceived);
	bool res = reconnect();
	logger.println(tag, String(F("\n\t <<initMQTTServer =")) + Logger::boolToString(res));
}

void loop()
{	
	//nexLoop(nex_listen_list);
	nextDisplay.loop();

#ifdef ESP8266
	ESP.wdtFeed();
#endif // ESP8266
	shield.setFreeMem(ESP.getFreeHeap());

	unsigned long currMillis = millis();
	if (currMillis - lastReboot > reboot_interval) {
		shield.setEvent(F("timeout reboot"));
		logger.println(tag, F("\n\n\n\-----------lastReboot TIMEOUT REBOOT--------\n\n"));
		reboot("daily reboot");
	}

	currMillis = millis();
	if (currMillis - shield.lastCheckHealth > shield.checkHealth_timeout) {
		shield.setEvent(F("check health reboot"));
		logger.println(tag, F("\n\n\n\-----------CHECK HEALTH TIMEOUT REBOOT--------\n\n"));
		//ESP.restart();
		reboot("Check health Timeout");
	}

#ifdef ESP8266
	wdt_enable(WDTO_8S);
#endif

	shield.checkStatus();

	if (client.connected()) {
		shield.checkTimeUpdateStatus();
		shield.checkSettingResquestStatus();
		mqttclient.loop();
	}
	else {
		logger.println(tag, F("\n\n\tSERVER DISCONNECTED!!!\n"));
		shield.setEvent(F("server disconnected"));
		reconnect();
		delay(5000);
	}
}
