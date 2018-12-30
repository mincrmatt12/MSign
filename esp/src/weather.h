#ifndef WEATHER_H
#define WEATHER_H

#include "common/slots.h"

namespace weather {
	void init();
	void loop();

	slots::WeatherInfo info;
	char * info_buffer;
	uint8_t buffer_size;
	uint8_t echo_index = 0;

	char icon[16];

	uint64_t time_since_last_update = 0;
}

#endif
