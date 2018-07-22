#ifndef _IRSENSOR_h
#define _IRSENSOR_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include "Sensor.h"
#include <Arduino.h>
#include "Logger.h"
#include "CommandResponse.h"

#ifdef ESP8266
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Daikin.h>
#endif

class IRSensor :
	public Sensor
{
private:
	static String tag;
	static Logger logger;

	virtual String getJSONFields();
#ifdef ESP8266
	IRsend *pirsend;
	IRDaikinESP *pdaikinir;
#endif
	bool sendCode(String codetype, uint64_t code, int bit);

	bool sendDaikin();
	bool sendSamsungTv();
	bool sendRobotClean();
	bool sendRobotHome();
	bool sendHarmanKardonDisc();

public:
	//const String STATUS_DOOROPEN = "dooropen";
	//const String STATUS_DOORCLOSED = "doorclosed";

	IRSensor(int id, uint8_t pin, bool enabled, String address, String name);
	~IRSensor();

	virtual void init();
	//virtual bool getJSON(JSONObject *jObject);
	virtual bool checkStatusChange();
	bool receiveCommand(String command, int id, String uuid, String json);
};
#endif
