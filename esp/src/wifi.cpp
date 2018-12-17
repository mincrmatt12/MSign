#include "wifi.h"

#include "time.h"
#include "common/slots.h"
#include "config.h"
#include "serial.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>

WiFiEventHandler got_ip, discon;
bool wifi_available = false;

void wifi::prepare() {
	WiFi.disconnect();
	WiFi.mode(WIFI_OFF);
	Serial1.println("Discon.");
}

void wifi::init() {
	// setup wifi events
	
	got_ip = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& e){
			// inform the stm
			wifi_available = true;
			serial::interface.update_data(slots::WIFI_STATUS, reinterpret_cast<uint8_t *>(&wifi_available), 1);
			time::start();
	});

	discon = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &e){
			// inform the stm
			wifi_available = false;
			serial::interface.update_data(slots::WIFI_STATUS, reinterpret_cast<uint8_t *>(&wifi_available), 1);
			time::stop();
	});

	// start up wifi with parameters loaded
	WiFi.begin(config::manager.get_value(config::SSID), 
			   config::manager.get_value(config::PSK));
}

bool wifi::available() {
	return wifi_available;
}
