#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "serial.h"
#include "config.h"
#include "wifi.h"
#include "time.h"
#include "ttc.h"
#include "weather.h"

void setup() {
	Serial.begin(115200, SERIAL_8E1);
	Serial1.begin(115200, SERIAL_8N1);
	Serial1.setDebugOutput(true);

	Serial.swap();

	Serial1.println(F("\nStarting Sign Backend."));
	Serial1.println(F("You are looking at the debug output."));

	wifi::prepare();
	serial::interface.ensure_handshake();
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
