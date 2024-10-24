#ifndef WIFI_H
#define WIFI_H

#include <FreeRTOS.h>
#include <portmacro.h>
#include <event_groups.h>
#include <time.h>

namespace wifi {
	bool init();

	extern EventGroupHandle_t events;
	uint64_t get_localtime();
	uint64_t millis_to_local(uint64_t millis);

	time_t timegm(tm const* t);
	uint64_t from_iso8601(const char * ts, bool treat_as_local = false, bool treat_midnight_as_end_of_day = false);

	enum Events : EventBits_t {
		WifiConnected = 1, // set when wifi is connected (unset when disconnected)
		TimeSynced = 2,    // set when sntp finishes
		StmConnected = 4,  // set when stm is connected
		GrabRequested = 8, // set to wakeup grabber for refreshing
		GrabTaskStop = 16, // if set, grab task should not be running (when unset, grab task is unmasked)
		GrabTaskDead = 32, // set when the grab task is stopped
	};
}

#endif
