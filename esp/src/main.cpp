#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SdFat.h>
#include "serial.h"
#include "config.h"
#include "wifi.h"
#include "time.h"
#include "ttc.h"
#include "weather.h"
	
SdFatSoftSpi<D6, D2, D5> sd;

void setup() {
	Serial.begin(115200, SERIAL_8E1);
	Serial1.begin(115200, SERIAL_8N1);
	Serial1.setDebugOutput(true);

	Serial.swap();

	Serial1.println(F("\nStarting Sign Backend."));
	Serial1.println(F("You are looking at the debug output."));

	wifi::prepare();
	serial::interface.ensure_handshake();

	if (!sd.begin(D1)) {
		Serial1.println(F("SD Card couldn't init.\n"));
		delay(1000);
		ESP.restart();
	}

	config::manager.load_from_sd();
	wifi::init();
	time::init();
	ttc::init();
	weather::init();
}

void loop() {
	serial::interface.loop();
	ttc::loop();
	wifi::loop();
	weather::loop();
	delay(1);
}
