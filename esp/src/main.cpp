#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "serial.h"
#include "config.h"

void setup() {
	Serial.begin(115200, SERIAL_8E1);
	Serial1.begin(115200, SERIAL_8N1);
	Serial1.setDebugOutput(true);

	Serial.swap();

	Serial1.println(F("\nStarting Sign Backend."));
	Serial1.println(F("You are looking at the debug output."));

	serial::interface.ensure_handshake();
	config::manager.load_from_sd();

	// start up wifi with parameters loaded
	
	WiFi.mode(WIFI_STA);
	WiFi.hostname("signesp");
	WiFi.begin(config::manager.get_value(config::SSID), 
			   config::manager.get_value(config::PSK));
}

void loop() {
	serial::interface.loop();
	delay(1);
}
