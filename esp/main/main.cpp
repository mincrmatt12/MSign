#include "serial.h"
#include "config.h"
#include "wifi.h"
#include "stime.h"
#include "upd.h"
#include "util.h"

extern "C" void app_main() {
	puts("Starting MSign backend");
}

/*
void setup() {
	Serial.begin(115200, SERIAL_8E1);
	Serial1.begin(115200, SERIAL_8N1);
	Serial1.setDebugOutput(true);

	Serial.setRxBufferSize(1024);
	Serial.swap();

	Serial1.println();
	Serial1.println();
	Serial1.println(F("\n==== Starting Sign Backend v2. ===="));
	Serial1.println(F("You are looking at the debug output."));

	wifi::prepare();

	if (!sd.begin(D1)) {
		Serial1.println(F("SD Card couldn't init.\n"));
		delay(1000);
		ESP.restart();
	}

	pinMode(D3, INPUT);
	delay(5);
	bool skip_update = digitalRead(D3) == 0;

	// check for updates
	if (!skip_update) {
		auto reason = upd::needed();
		if (reason == upd::WEB_UI) {
			upd::update_website();
			ESP.restart();
		}
		else if (reason == upd::FULL_SYSTEM) {
			upd::update_system();
			Serial1.println(F("inv"));
		}
	}
	else {
		Serial1.println(F("skipping update due to FLASH hold"));
	}

	config::manager.load_from_sd();
	while (!serial::interface.ensure_handshake()) {delay(1);}

	randomSeed(secureRandom(ESP.getVcc() + ESP.getCycleCount()));

	wifi::init();
	debug::init();
	serial::interface.register_debug_commands();
	signtime::init();
	ttc::init();
	weather::init();
	webui::init();
	sccfg::init();
	calfix::init();
	modelserve::init();
}

void loop() {
	serial::interface.loop();
	ttc::loop();
	wifi::loop();
	weather::loop();
	webui::loop();
	sccfg::loop();
	calfix::loop();
	modelserve::loop();
	debug::loop();
	Log.dump();
	delay(1);
}
*/
