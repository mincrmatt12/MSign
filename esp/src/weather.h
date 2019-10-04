#ifndef WEATHER_H
#define WEATHER_H

#include "common/slots.h"

namespace weather {
	void init();
	void loop();

	extern slots::WeatherInfo info;
	extern char * info_buffer;
	extern uint8_t buffer_size;
	extern uint8_t echo_index;
	extern char icon[16];
	extern uint64_t time_since_last_update;

	extern float temp_over_day[24];
	extern slots::WeatherStateArrayCode state_data[24];
}

#endif
