#include <Arduino.h>

void setup() {
	Serial.swap();
	Serial1.setDebugOutput(true);
	Serial1.begin(115200, SERIAL_8E1);

	Serial1.println("Starting Sign Backend.");
	Serial1.println("You are looking at the debug output.");
}

void loop() {
}
