#ifndef MSIGN_MSR_H
#define MSIGN_MSR_H

#include "grab.h"

namespace modelserve {
	void init();
	bool loop();

	constexpr static auto modelserve_grabber = grabber::make_http_grabber(init, loop, pdMS_TO_TICKS(100*1000));
}

#endif
