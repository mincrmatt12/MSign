#include "serial.h"
#include "config.h"
#include "wifitime.h"
#include "upd.h"
#include "sd.h"
#include "util.h"
#include "config.h"
#include "grabber/grab.h"

#include <esp_log.h>
#include <esp_system.h>
#include <FreeRTOS.h>
#include <task.h>

const static char * TAG = "app_main";

extern "C" void app_main() {
	ESP_LOGI(TAG, "Starting MSign...");

	// Try and start the SD layer
	switch (sd::init()) {
		case sd::InitStatus::NoCard:
			ESP_LOGE(TAG, "=============================");
			ESP_LOGE(TAG, "No card is inserted! Halting!");
			ESP_LOGE(TAG, "=============================");
			return;
		case sd::InitStatus::UnusableCard:
		case sd::InitStatus::UnkErr:
			ESP_LOGE(TAG, "Unusable/unknown error while initializing the card.");
			return;
		case sd::InitStatus::FormatError:
			ESP_LOGE(TAG, "No filesystem on card");
			// TODO: maybe indicate these failures to the STM somehow?
			return;
		default:
			break;
	}

	// Load the config from the SD card
	if (!config::manager.load_from_sd()) {
		return;
	}

	// Start up the servicer
	xTaskCreate((TaskFunction_t)&serial::SerialInterface::run, "srv", 3072, &serial::interface, 9, NULL);

	if (!wifi::init()) {
		ESP_LOGE(TAG, "Not starting data services due to no WIFI.");
		// TODO: make this setup softap mode for netcfg
	}

	// Start the grabber
	xTaskCreate(                                grabber::run, "grab", 7680, nullptr,            5, NULL);

	ESP_LOGI(TAG, "Created tasks");
	ESP_LOGI(TAG, "Free heap available is %d", esp_get_free_heap_size());
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
