#pragma once

#include "grab.h"
#include "../common/slots.h"

namespace parcels {
	// This is both a grabber and module for other grabbers to tie into.
	void init();
	bool loop();

	constexpr static auto parcels_grabber = grabber::make_refreshable_grabber(
			grabber::make_https_grabber(init, loop, pdMS_TO_TICKS(12*60*1000), pdMS_TO_TICKS(30*1000)),
			slots::protocol::GrabberID::PARCELS
	);

	// Request to create a new tracker id; all values are malloced and autofreed
	char * generate_tracker_id(char * tracker_code, char * tracker_carrier, bool& had_error);
}