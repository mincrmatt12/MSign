#ifndef WIFI_H
#define WIFI_H

#include <FreeRTOS.h>
#include <portmacro.h>
#include <event_groups.h>

namespace wifi {
	bool init();

	extern EventGroupHandle_t events;

	enum Events : EventBits_t {
		WifiConnected = 1,
		TimeSynced = 2
	};
}

#endif
