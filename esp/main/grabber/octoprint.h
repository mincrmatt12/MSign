#pragma once

#include "grab.h"

namespace octoprint {
	void init();
	bool loop();

	constexpr static auto octoprint_grabber = grabber::make_refreshable_grabber(
			grabber::make_http_grabber(init, loop, pdMS_TO_TICKS(60*1000), pdMS_TO_TICKS(60*1000)),
			slots::protocol::GrabberID::PRINTER
	);
}
