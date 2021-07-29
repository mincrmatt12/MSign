#ifndef MSIGN_CFGPL_H
#define MSIGN_CFGPL_H

#include "grab.h"

namespace cfgpull {
	void init();
	bool loop();

	// Check every 2 minutes for a new config update
	constexpr static auto cfgpull_grabber = grabber::make_https_grabber(init, loop, pdMS_TO_TICKS(120*1000));
}

#endif
