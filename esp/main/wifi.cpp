#include "wifi.h"

#include "util.h"
#include "stime.h"
#include "common/slots.h"
#include "config.h"
#include "serial.h"

bool client_ok = false;
bool wifi_available = false;

void wifi::prepare() {
}

void wifi::init() {
	// setup wifi events
	
	/*
	got_ip = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& e){
			// inform the stm
			wifi_available = true;
			serial::interface.update_data(slots::WIFI_STATUS, reinterpret_cast<uint8_t *>(&wifi_available), 1);
			signtime::start();
	});

	discon = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &e){
			// inform the stm
			wifi_available = false;
			serial::interface.update_data(slots::WIFI_STATUS, reinterpret_cast<uint8_t *>(&wifi_available), 1);
			signtime::stop();
	});

	// start up wifi with parameters loaded
	WiFi.persistent(false);
	WiFi.mode(WIFI_STA);
	WiFi.hostname("msign");
	WiFi.begin(config::manager.get_value(config::SSID), 
			   config::manager.get_value(config::PSK));

	ctrl_server.begin();
	*/
}

bool wifi::available() {
	return wifi_available;
}
