#ifndef GTFS_H
#define GTFS_H

#include "../grab.h"

namespace transit::gtfs {
	void init();
	bool loop();

	// Update every minute
	constexpr static auto gtfs_grabber = grabber::make_https_grabber(init, loop, pdMS_TO_TICKS(60*1000));
}

#endif
