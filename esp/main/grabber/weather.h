#ifndef WEATHER_H
#define WEATHER_H

#include "grab.h"

namespace weather {
	void init();
	bool loop();

	// Rate limit is 1000/day, so we use a conservative 5 minutes
	constexpr static auto weather_grabber = grabber::make_https_grabber(init, loop, pdMS_TO_TICKS(60*5*1000), pdMS_TO_TICKS(60*3*1000));
}

#endif
