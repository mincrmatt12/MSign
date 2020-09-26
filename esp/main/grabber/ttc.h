#ifndef TTC_H
#define TTC_H

#include "grab.h"

namespace ttc {
	void init();
	bool loop();

	// There's no rate limit so....
	constexpr static auto ttc_grabber = grabber::make_http_grabber(init, loop, pdMS_TO_TICKS(45000));
}

#endif
