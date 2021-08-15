#ifndef MSIGN_CFGPL_H
#define MSIGN_CFGPL_H

#include "grab.h"

namespace cfgpull {
	void init();
	bool loop();

	// Check every 5 minutes for a new config update
	constexpr static auto cfgpull_grabber = grabber::make_refreshable_grabber(
			grabber::make_https_grabber(init, loop, pdMS_TO_TICKS(5*60*1000)),
			slots::protocol::GrabberID::CFGPULL
	);
}

#endif
