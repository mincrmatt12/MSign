#include "weather.h"
#include "config.h"
#include "serial.h"
#include "util.h"
#include "json.h"
#include "time.h"

void weather::init() {
	weather::info_buffer = (char*)malloc(0);
	weather::buffer_size = 0;
	weather::echo_index = 0;

	memset(&weather::info, 0, sizeof(weather::info));
	memset(weather::icon, 0, 16);
}

void weather::loop() {
	if (time_since_last_update == 0 || (time::get_time() - time_since_last_update) > 240000) {
		// Do a weather update.
	}
}
