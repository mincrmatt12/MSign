#ifndef MSIGN_SC_H
#define MSIGN_SC_H

#include "grab.h"
#include "../common/slots.h"

namespace sccfg {
	// This is both a grabber and module for other grabbers to tie into.
	void init();
	bool loop();

	void disable_screen(slots::ScCfgInfo::EnabledMask e);
	void enable_screen(slots::ScCfgInfo::EnabledMask e);

	constexpr static auto sccfg_grabber = grabber::make_https_grabber(init, loop, pdMS_TO_TICKS(30*1000));
}

#endif
