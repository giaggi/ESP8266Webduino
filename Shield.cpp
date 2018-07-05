#include "Shield.h"
#include "DoorSensor.h"
#include "HornSensor.h"
#include "Logger.h"
#include "HeaterActuator.h"
#include "Command.h"
#include "ESP8266Webduino.h"
#include "JSONArray.h"
#include <Adafruit_ST7735.h>
#include "SensorFactory.h"

extern void writeEPROM();
extern void resetEPROM();
extern void resetWiFiManagerSettings();
//extern String siid;
//extern String pass;

extern bool mqtt_publish(String topic, String message);
extern bool _mqtt_publish(char* topic, char* payload);

Logger Shield::logger;
String Shield::tag = "Shield";

String Shield::lastRestartDate = "";
String Shield::swVersion = "1.48";
int Shield::id = 0; //// inizializzato a zero perch� viene impostato dalla chiamata a registershield

// default shield setting
//String Shield::networkSSID = "TP-LINK_3BD796";
//String Shield::networkPassword = "giacomocasa";
int Shield::localPort = 80;
String Shield::serverName = "192.168.1.33";
String Shield::mqttServer = "192.168.1.33";
int Shield::mqttPort = 1883;
int Shield::serverPort = 8080;
String Shield::shieldName = "shieldName";

unsigned char Shield::MAC_array[6];
char Shield::MAC_char[18];
bool Shield::mqttMode = true;// true;
bool Shield::configMode = true;// true;
bool Shield::resetSettings = false;// true;
String Shield::powerStatus = "on"; // da aggiungere

Shield::Shield()
{
	//phearterActuator = new HeaterActuator(0, true, "heaterxxxx", "heater");
	lastCheckHealth = 0;
}

Shield::~Shield()
{
	// E' inutile perchè non lo chiamerà mai
	sensorList.clearAll();
}

void Shield::init() {

	tftDisplay.init();
	//display.init();
}

void Shield::clearAllSensors() {

	sensorList.clearAll();
}

Sensor* Shield::getSensorFromAddress(String addr) {
	for (int i = 0; i < sensorList.count; i++) {
		Sensor* sensor = (Sensor*)sensorList.get(i);
		if (sensor->address == addr)
			return sensor;
	}
	return nullptr;
}

Sensor* Shield::getSensorFromId(int id) { /// sidsogna aggiungere anche richerca nei child
	logger.print(tag, "\n\t >>getSensorFromId " + String(id));
	for (int i = 0; i < sensorList.count; i++) {
		/*logger.print(tag, "\n\t sendor id= " + String(id));

		Sensor* sensor = (Sensor*)sensorList.get(i);
		logger.print(tag, "\n\t sensor id= " + String(sensor->sensorid));

		return sensor->getSensorFromId(id);*/
		
		Sensor* sensor = (Sensor*)sensorList.get(i);
		if (sensor->sensorid == id) {
			return sensor;
		}

		for (int k = 0; k < sensor->childsensors.length(); k++) {
			logger.print(tag, "\n\t k= " + String(k));
			Sensor* child = (Sensor*)sensor->childsensors.get(k);
			logger.print(tag, "\n\t childsensors= " + String(child->sensorid));
			logger.print(tag, "\n\t name= " + String(child->sensorname));
			if (child->sensorid == id)
				return (Sensor*)child;

		}

	}
	logger.println(tag, "\n\t <<getSensorFromId: sensornotfound");
	return nullptr;
}


void Shield::drawString(int x, int y, String txt, int size, int color) {

	//String str = "prova";
	tftDisplay.drawString(x, y, txt, 1, ST7735_WHITE);
	//tftDisplay.drawString(int row, int col, String txt, int size, int color);
}

/*Just as a heads up, to use the "drawXBitmap" function, I also had
to redefine the array from "static unsigned char" to "static const uint8_t PROGMEM"
in my image file.If I just used the original file, then garbage would appear on my 32x32 gird.
I think the array has to be defined as "PROGMEM" because the "drawXBitmap" function uses the
"pgm_read_byte" function, which reads a byte from program memory.I also had to include
the line "#include <avr/pgmspace.h>".After that, everything worked fine for me.*/

//#include <avr/pgmspace.h>
#define temperature_width 32
#define temperature_height 32
static const uint8_t PROGMEM temperature_bits[] = {
	0x00, 0x80, 0x01, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0x70, 0x0e, 0x00,
	0x00, 0x18, 0x18, 0x00, 0x00, 0x08, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00,
	0x00, 0x18, 0x10, 0x00, 0x00, 0x78, 0x10, 0x00, 0x00, 0x18, 0x10, 0x00,
	0x00, 0x18, 0x10, 0x00, 0x00, 0x78, 0x10, 0x00, 0x00, 0x18, 0x10, 0x00,
	0x00, 0x08, 0x10, 0x00, 0x00, 0xc8, 0x13, 0x00, 0x00, 0xc8, 0x13, 0x00,
	0x00, 0xc8, 0x13, 0x00, 0x00, 0xc8, 0x13, 0x00, 0x00, 0xc8, 0x13, 0x00,
	0x00, 0xc8, 0x13, 0x00, 0x00, 0xc8, 0x13, 0x00, 0x00, 0xcc, 0x33, 0x00,
	0x00, 0xc4, 0x23, 0x00, 0x00, 0xe6, 0x67, 0x00, 0x00, 0xe2, 0x67, 0x00,
	0x00, 0xe6, 0x67, 0x00, 0x00, 0xe6, 0x67, 0x00, 0x00, 0xe6, 0x67, 0x00,
	0x00, 0xce, 0x71, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x3c, 0x3c, 0x00,
	0x00, 0xf0, 0x0f, 0x00, 0x00, 0xc0, 0x03, 0x00 };

void Shield::clearScreen() {
	tftDisplay.clear();

	tftDisplay.drawXBitmap(90, 50, temperature_bits, temperature_width, temperature_height, 0xF800/*ST3577_RED*/);
}

bool Shield::onUpdateSensorListCommand(JSON& json) {

	logger.print(tag, "\n\t >>onUpdateSensorListCommand");
	sensorList.clearAll();

	if (json.has("sensors")) {

		String str = json.jsonGetArrayString("sensors");
		logger.print(tag, "\n\t str=" + str);
		JSONArray jArray(str);
		String item = jArray.getFirst();



		SensorFactory factory;
		clearAllSensors(); // serve per inizializzare
		int i = 0;
		while (!item.equals("")) {

			logger.print(tag, "\n\n\t SENSOR: #" + String(i++));
			logger.print(tag, " =" + item);

			JSONObject jObject(item);
			logger.print(tag, "json=" + jObject.toString());
			//logger.print(tag, "json=" + logger.formattedJson(jObject.toString()));

			Sensor* sensor = factory.createSensor(&jObject);
			if (sensor != nullptr) {
				//addSensor(sensor);
				sensorList.add(sensor);
			}

			item = jArray.getNext();
		}

		sensorList.show();

		writeEPROM();

	}

	logger.print(tag, "\n\t <<onUpdateSensorListCommand");
	return true;
}


bool Shield::receiveCommand(String jsonStr) {

	logger.print(tag, "\n");
	logger.println(tag, ">>receiveCommand\njson=" + jsonStr);

	JSON json(jsonStr);
	if (json.has("command")) {

		bool result = false;
		String command = json.jsonGetString("command");
		if (command.equals("updatesensorlist")) { // ???
			logger.print(tag, "\n\t ++updatesensorlist");
			result = onUpdateSensorListCommand(json);
			logger.print(tag, "\n\t <<sendCommand result=" + String(result));
			return result;
		}
		else if (command.equals("shieldsettings")) { // risposta a loadsettting
			logger.print(tag, "\n\t ++shieldsettings");
			result = onShieldSettingsCommand(json);
			logger.print(tag, "\n\t <<sendCommand result=" + String(result));
			return result;
		}
		else if (command.equals("power")) { // ??
			logger.print(tag, "\n\t ++power");
			result = onPowerCommand(json);
			logger.print(tag, "\n\t <<sendCommand result=" + String(result));
			return result;
		}
		else if (command.equals("reset")) { // che differenza c'è tra reboot e reset?
			logger.print(tag, "\n\t ++reset");
			result = onResetCommand(json);
			logger.print(tag, "\n\t <<sendCommand result=" + String(result));
			result = onRebootCommand(json);
			logger.print(tag, "\n\t <<onRebootCommand result=" + String(result));
			return result;
		}
		else if (command.equals("reboot")) { // che differenza c'è tra reboot e reset?
			logger.print(tag, "\n\t ++reboot");
			result = onRebootCommand(json);
			logger.print(tag, "\n\t <<onRebootCommand result=" + String(result));
			return result;
		}
		else if (command.equals("updatesensorstatus")) { // questo può essere eliminato??
			// richiesta stato di tutti i sensori
			logger.print(tag, "\n\t ++updatesensorstatus");
			
			// uuid
			String uuid = "";
			if (json.has("uuid")) {
				uuid = json.jsonGetString("uuid");
				logger.print(tag, "\n\t uuid=" + uuid);
			}
			else {
				logger.print(tag, "\n\t uuid NOT FOUND!");
			}

			
			String topic = "toServer/response/" + uuid + "/success";
			//bool res = mqtt_publish(topic, getSensorsStatusJson());

			char stopic[200];
			int i;
			for (int i = 0; i < topic.length(); i++) {
				stopic[i] = topic.charAt(i);
			}
			stopic[i] = '\0';
			
						
			char payload[600];
			bool res = _getSensorsStatusJson(payload);
			res = _mqtt_publish(stopic, payload);

			logger.print(tag, "\n\t <<sendupdatesensorstatus " + uuid);
			return res;
			
		}
		if (command.equals("checkhealth")) {
			logger.print(tag, "\n\t ++checkhealth");

			// uuid
			String uuid = "";
			if (json.has("uuid")) {
				uuid = json.jsonGetString("uuid");
				logger.print(tag, "\n\t uuid=" + uuid);
			}
			else {
				logger.print(tag, "\n\t uuid NOT FOUND!");
				return false;
			}

			lastCheckHealth = millis();
			String topic = "toServer/response/" + uuid + "/success";

			bool res = mqtt_publish(topic, getJson());

			logger.print(tag, "\n\t --checkhealth " + uuid);
			return res;
		}
		/*else if (command.equals("register")) {
			logger.print(tag, "\n\t ++register");
			bool result = sendRegister();
			logger.print(tag, "\n\t <<sendCommand result=" + String(result));
			return result;
		}*/
		else if (json.has("actuatorid") /*command.equals("requestsensorstatus")*/ && json.has("uuid")) {
			
			int id = json.jsonGetInt("actuatorid");
			String uuid = json.jsonGetString("uuid");
			Sensor* sensor = getSensorFromId(id);
			if (sensor != nullptr) {				
				String topic = "toServer/response/" + uuid + "/success";
				bool res = mqtt_publish(topic, sensor->getJSON());
				return sensor->receiveCommand(command,id,uuid, jsonStr);
			}

			/*logger.print(tag, "\n\t ++requestsensorstatus");
			int id = json.jsonGetInt("actuatorid");
			Sensor* sensor = getSensorFromId(id);
			if (sensor != nullptr) {
				logger.print(tag, "\n\t sensorname=" + sensor->sensorname);

				String uuid = json.jsonGetString("uuid");
				String topic = "toServer/response/" + uuid + "/success";
				bool res = mqtt_publish(topic, sensor->getJSON());
				return res;
				
			}*/
		}
		/*else if (json.has("actuatorid")) { // se contiene "actuatorid" vuole dire che
											// è un comando diretto ad un sensore e quindi
											// chiama la sensor->receivecommand

			logger.print(tag, "\n\t ++actuator command");
			int id = json.jsonGetInt("actuatorid");
			Sensor* sensor = getSensorFromId(id);
			if (sensor != nullptr) {
				logger.print(tag, "\n\t sensorname=" + sensor->sensorname);
				logger.print(tag, "\n\t addr=" + String(sensor->address));
				CommandResponse result = sensor->receiveCommand(jsonStr);

				if (result.result.equals("success")) {
					String topic = "toServer/response/" + result.uuid + "/success";
					bool res = mqtt_publish(topic, sensor->getJSON());
					return res;
				}
				else {
					return false;
				}
			}			
		}*/
		
	}
	return false;
}

bool Shield::onShieldSettingsCommand(JSON& json)
{
	logger.print(tag, "\n\t>>onShieldSettingsCommand");

	if (json.has("localport")) {
		int localPortr = json.jsonGetInt("localport");
		logger.print(tag, "\n\t localport=" + localPort);
		setLocalPort(localPort);
	}
	if (json.has("shieldname")) {
		String name = json.jsonGetString("shieldname");
		logger.print(tag, "\n\t shieldname=" + name);
		setShieldName(name);
	}
	/*if (json.has("ssid")) {
		String name = json.jsonGetString("ssid");
		logger.print(tag, "\n\t ssid=" + name);
		setNetworkSSID(name);
	}
	if (json.has("password")) {
		String password = json.jsonGetString("password");
		logger.print(tag, "\n\t password=" + password);
		setNetworkPassword(password);
	}*/
	if (json.has("servername")) {
		String name = json.jsonGetString("servername");
		logger.print(tag, "\n\t servername=" + name);
		setServerName(name);
	}
	if (json.has("serverport")) {
		int serverPort = json.jsonGetInt("serverport");
		logger.print(tag, "\n\t serverport=" + serverPort);
		setServerPort(serverPort);
	}
	/*if (json.has("mqttserver")) {
		String mqttserver = json.jsonGetString("mqttserver");
		logger.print(tag, "\n\t mqttserver=" + mqttserver);
		setMQTTServer(mqttserver);
	}
	if (json.has("mqttport")) {
		int mqttport = json.jsonGetInt("mqttport");
		logger.print(tag, "\n\t mqttport=" + mqttport);
		setMQTTPort(mqttport);
	}*/
	writeEPROM();

	/*String result = "";
	result += "{";
	result += "\"result\": \"succes\"";

	result += ",\"localport\": " + String(getLocalPort());
	result += ",\"shieldname\": \"" + getShieldName() + "\"";
	//result += ",\"ssid\": \"" + getNetworkSSID() + "\"";
	//result += ",\"password\": \"" + getNetworkPassword() + "\"";
	result += ",\"mqttserver\": \"" + getMQTTServer() + "\"";
	result += ",\"mqttport\": " + String(getMQTTPort());
	result += ",\"servername\": \"" + getServerName() + "\"";
	result += ",\"serverport\": " + String(getServerPort());

	result += "}";*/

	logger.print(tag, "\n\t<<onShieldSettingsCommand");
	return true;

}

/*bool Shield::sendRegister()
{
	logger.print(tag, "\n\t>> sendRegister");

	Command command;
	String json = getJson();
	command.registerShield(json);


	return true;
}*/

bool Shield::onRebootCommand(JSON& json)
{
	logger.print(tag, "\n\t>> onRebootCommand");
	ESP.restart();
	return true;
}

bool Shield::sendUpdateSensorStatus()
{
	logger.print(tag, "\n");
	logger.println(tag, ">> sendUpdateSensorStatus");

	Command command;
	logger.print(tag, "\n\n\t SEND SENSOR STATUS UPDATE\n");

	logger.println(tag, ">>command.sendSensorsStatus");
	//String json = getSensorsStatusJson();
	//bool res = command.sendSensorsStatus(json);

	char payload[1000];
	bool result = _getSensorsStatusJson(payload);
	bool res = command._sendSensorsStatus(payload);
	
	logger.println(tag, "<< sendUpdateSensorStatus\n");

	return res;
}



/*void Shield::registerShield()
{
	logger.print(tag, "\n");
	logger.println(tag, ">>registerShield");

	Command command;
	String json = getJson();
	logger.println(tag, "json=" + logger.formattedJson(json));
	command.registerShield(json);

	logger.println(tag, "<<registerShield");
}*/

bool Shield::onResetCommand(JSON& json)
{
	logger.print(tag, "\n\t>> onResetCommand");
	resetWiFiManagerSettings();
	resetEPROM(); // scrive 0 nel primo settore della EPROM per azzerare tutto

	return true;
}

bool Shield::onPowerCommand(JSON& json)
{
	logger.print(tag, "\n\t>> onPowerCommand");
	bool res = false;

	if (json.has("status")) {
		String status = json.jsonGetString("status");
		logger.print(tag, "\n\t status=" + status);

		if (status.equals("on")) {
			setPowerStatus(status);
			res = true;
		}
		else if (status.equals("off")) {
			setPowerStatus(status);
			res = true;
		}
	}

	logger.print(tag, "\n\t<< onPowerCommand");
	return true;
}

String Shield::getJson() { 

	logger.print(tag, "\n\n");
	logger.println(tag, ">>getJson");

	// andrebbe trasformato in JSONObject
	String str = "{";
	
	str += "\"MAC\":\"" + String(MAC_char) + "\"";
	str += ",\"swversion\":\"" + swVersion + "\"";
	str += ",\"lastreboot\":\"" + lastRestartDate + "\"";
	str += ",\"lastcheckhealth\":\"" + String(lastCheckHealth) + "\"";
	str += ",\"localIP\":\"" + localIP + "\"";
	str += ",\"localPort\":\"" + String(Shield::getLocalPort()) + "\"";

	//logger.print(tag, "\n\tstr=" + str + "\n");

	// sensori
	/*str += ",\"sensors\":[";

	logger.print(tag, "\n\t shield.sensorList.count=" + String(sensorList.count) + "\n");
	sensorList.show();

	for (int i = 0; i < sensorList.count; i++) {
		Sensor* sensor = (Sensor*)sensorList.get(i);
		if (sensor != nullptr) {
			if (i != 0)
				str += ",";
			JSONObject json;
			sensor->getJSON(&json);
			str += json.toString();
			logger.print(tag, "\n\tstr=" + str + "\n");
		}
	}
	str += "]";*/
	//str += "}";
	str += "}";

	logger.print(tag, "\n\tstr=" + str + "\n");

	logger.println(tag, "<<getJson");
	return str;
}



bool Shield::_getSensorsStatusJson(char *payload) {

	Serial.println(ESP.getFreeHeap());

	logger.print(tag, "\n\n");
	logger.println(tag, F(">>_getSensorsStatusJson"));

	sensorList.show();

	sprintf(payload, "{\"shieldid\":%d,\"swversion\":\"%s\",\"sensors\":[", id, Shield::swVersion.c_str());
		
	int payloadcount = strlen(payload);
		
	for (int i = 0; i < sensorList.count; i++) {
		Sensor* sensor = (Sensor*)sensorList.get(i);
		
		if (i != 0)
			payload[payloadcount++] = ',';
		String str = sensor->getJSON();
		for (int i = 0; i < str.length(); i++) {
			payload[payloadcount++] = str.charAt(i);
		}
	}
	
	payload[payloadcount++] = ']';
	payload[payloadcount++] = '}';
	payload[payloadcount++] = '\0';
	Serial.print("payload=");
	Serial.print(payload);
	Serial.print("--Endpayload");
	Serial.println(payloadcount,DEC);
	logger.println(tag, "<<_getSensorsStatusJson = ");
	return payload;
}



String Shield::getSensorsStatusJson() {

	logger.print(tag, "\n\n");
	logger.println(tag, F(">>getSensorsStatusJson"));

	sensorList.show();

	String json = "{";
	json += "\"shieldid\":" + String(id);// shieldid
	json += ",\"swversion\":\"" + Shield::swVersion + "\"";// shieldid
	//json += ",\"power\":\"" + Shield::powerStatus + "\"";// shieldid

	json += ",\"sensors\":[";

	

	for (int i = 0; i < sensorList.count; i++) {
		Sensor* sensor = (Sensor*)sensorList.get(i);

	
		if (i != 0)
			json += ",";
			
		json += sensor->getJSON();

		logger.println(tag, "\n\n\t json length=" + String(json.length()));
	}
	json += "]";
	json += "}";

	logger.println(tag, "<<getSensorsStatusJson = ");
	return json;
}

String Shield::getSettingsJson() { // usata per le impostazioni da jscript pages
	String json = "{";

	json += "\"localport\":" + String(localPort);
	json += ",\"shieldname\":\"" + shieldName + "\"";
	//json += ",\"ssid\":\"" + networkSSID + "\"";
	//json += ",\"password\":\"" + networkPassword + "\"";
	json += ",\"servername\":\"" + serverName + "\"";
	json += ",\"serverport\":" + String(serverPort);
	json += ",\"mqttserver\":\"" + mqttServer + "\"";
	json += ",\"mqttport\":" + String(mqttPort);
	json += ",\"localip\":\"" + String(localIP) + "\"";
	json += ",\"macaddress\":\"" + String(MAC_char) + "\"";
	json += ",\"shieldid\":" + String(id);
	json += ",\"datetime\":\"" + Logger::getStrDate() + "\"";
	json += ",\"power\":\"" + powerStatus + "\"";
	json += ",\"lastrestart\":\"" + lastRestartDate + "\"";
	json += ",\"heap\":\"" + String(ESP.getFreeHeap()) + "\"";
	json += ",\"swversion\":\"" + swVersion + "\"";

	json += "}";
	return json;
}

void Shield::checkStatus()
{
	//checkActuatorsStatus();
	checkSensorsStatus();

	/*display.clear();
	uint32_t getVcc = ESP.getVcc();// / 1024;
	String voltage = "Vcc " + String(getVcc) + "V";

	display.drawString(0, 0, Logger::getStrTimeDate() + " ", ArialMT_Plain_16);
	display.drawString(20, 16, Logger::getStrDayDate() + " ", ArialMT_Plain_10);
	display.drawString(0, 26, hearterActuator.getStatusName() + " " + String(hearterActuator.getReleStatus()) + voltage, ArialMT_Plain_10);
	for (int i = 0; i < sensorList.count; i++) {
		DS18S20Sensor* pSensor = (DS18S20Sensor*)sensorList.get(i);
		display.drawString(0, 16 + 10 + 10 + 10*(i), pSensor->sensorname + " " + pSensor->getTemperature(), ArialMT_Plain_10);
	}
	display.update();*/


	String date = Logger::getStrDate();
	//logger.println(tag, "dater:" + date);
	//if (!date.equals(oldDate)) {
		//tftDisplay.drawString(0, 0, Logger::getStrTimeDate(), 1, ST7735_WHITE);
		//tftDisplay.drawString(20, 16, Logger::getStrDayDate() + " ", 1, ST7735_WHITE);
	//}
	//oldDate = date;

	//tftDisplay.clear();
	int textWidth = 5;
	int textHeight = 8;

	tftDisplay.drawString(0, 0, Logger::getStrDayDate() + " ", 1, ST7735_WHITE);
	tftDisplay.drawString(0, textHeight, Logger::getStrTimeDate() + " ", 2, ST7735_WHITE);

	//tftDisplay.drawString(0, textHeight*(2 + 1), phearterActuator->getStatusName(), 2, ST7735_WHITE);

	String releStatus = "spento";
	/*if (phearterActuator->getReleStatus() == 1)
		releStatus = "acceso";
	tftDisplay.drawString(0, textHeight*(4 + 1), releStatus, 2, ST7735_WHITE);
	*/
	for (int i = 0; i < sensorList.count; i++) {
		Sensor* sensor = (Sensor*)sensorList.get(i);
		if (!sensor->type.equals("temperature"))
			continue;
		//DS18S20Sensor* pSensor = (DS18S20Sensor*)sensorList.get(i);
		//tftDisplay.drawString(0, textHeight * (2 + 1 + 2 + 2) + textHeight * i * 1, pSensor->sensorname + " " + pSensor->getTemperature(), 0, ST7735_WHITE);
	}
	//display.update();
}

void Shield::checkSensorsStatus()
{
	//logger.println(tag, ">>checkSensorsStatus" + String(sensorList.count));

	bool sensorStatusChanged = false;

	for (int i = 0; i < sensorList.count; i++) {

		Sensor* sensor = (Sensor*)sensorList.get(i);
		if (!sensor->enabled)
			continue;
		//logger.print(tag, "\n\t sensor->name: " + String(sensor->sensorname));
		bool res = sensor->checkStatusChange();

		unsigned long currMillis = millis();
		unsigned long timeDiff = currMillis - sensor->lastUpdateStatus;

		// Invia il comando se è cambiato lo stato del sensore
		// oppure se l’ultimo invio è fallito 
		// oppure se è passato il timeout dall’ultimo invio
		if (res || sensor->lastUpdateStatusFailed || timeDiff > sensor->updateStatus_interval) {
			sensorStatusChanged = true;
			sensor->lastUpdateStatus = millis();

			Command command;
			bool res = command.sendSensorStatus(sensor->getJSON());
			sensor->lastUpdateStatusFailed = !res;
		}

	}


	if (false/*sensorStatusChanged*/) {
		Command command;

		logger.println(tag, "\t >>checkSensorsStatus() SEND SENSOR STATUS UPDATE\n");

		/*String json = getSensorsStatusJson();
		logger.println(tag, "\n json=" + json + "\n");
		command.sendSensorsStatus(json);*/

		char payload[1000];
		bool res  = _getSensorsStatusJson(payload);
		//logger.println(tag, "\n json=" + json + "\n");
		command._sendSensorsStatus(payload);

		logger.println(tag, "\t <<checkSensorsStatus() SEND SENSOR STATUS UPDATE\n");


	}
}

void Shield::addSensor(Sensor* pSensor) {

	logger.print(tag, "\n\t >>addSensor " + pSensor->sensorname);

	sensorList.add((Sensor*)pSensor);
	//sensorList.show();
	logger.print(tag, "\n\t <<addSensor");
}