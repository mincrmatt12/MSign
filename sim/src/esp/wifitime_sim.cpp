#include "wifitime.h"
#include <esp_log.h>
#include "common/slots.h"
#include "serial.h"

bool wifi::init() {
	// Init events
	ESP_LOGW("wifiman", "Fake wifi init: ok");

	slots::WifiStatus fakestatus;
	fakestatus.connected = true;
	fakestatus.ipaddr[0] = 192;
	fakestatus.ipaddr[1] = 168;
	fakestatus.ipaddr[2] = 122;
	fakestatus.ipaddr[3] = 2;

	fakestatus.gateway[0] = 192;
	fakestatus.gateway[1] = 168;
	fakestatus.gateway[2] = 122;
	fakestatus.gateway[3] = 1;

	serial::interface.update_slot(slots::WIFI_STATUS, fakestatus);

	xEventGroupSetBits(events, wifi::WifiConnected|wifi::TimeSynced);

	return true;
}
