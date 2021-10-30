#ifndef WIFI_H
#define WIFI_H

#include <FreeRTOS.h>
#include <portmacro.h>
#include <event_groups.h>

namespace wifi {
	bool init();

	extern EventGroupHandle_t events;
	uint64_t get_localtime();
	uint64_t millis_to_local(uint64_t millis);

	enum Events : EventBits_t {
		WifiConnected = 1,
		TimeSynced = 2,
		StmConnected = 4,
		GrabRequested = 8,
		GrabTaskStop = 16, // set when the task should stop
	};
}

#endif
