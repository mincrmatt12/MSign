#include <Arduino.h>
#include "serial.h"

void setup() {
	Serial1.setDebugOutput(true);

	Serial.begin(115200, SERIAL_8E1);
	Serial1.begin(115200, SERIAL_8N1);

	Serial.swap();

	Serial1.println("Starting Sign Backend.");
	Serial1.println("You are looking at the debug output.");

	serial::interface.ensure_handshake();
}

void loop() {
}
