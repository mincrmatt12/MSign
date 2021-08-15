#ifndef MSIGN_MSR_H
#define MSIGN_MSR_H

#include "grab.h"

namespace modelserve {
	void init();
	bool loop();

	// Every 200 seconds (or 3m20 seconds) we change the model
	constexpr static auto modelserve_grabber = grabber::make_refreshable_grabber(
			grabber::make_http_grabber(init, loop, pdMS_TO_TICKS(200*1000)),
			slots::protocol::GrabberID::MODELSERVE
	);
}

#endif
