#ifndef TDSB_G_H
#define TDSB_G_H

#include "grab.h"

namespace tdsb {
	void init();
	bool loop();

	// This _really_ shouldn't change much, and it really doesn't matter if this takes a bit to update at midnight; so we set the
	// delay to like 15 minutes.
	//
	// The unrecoverable error timeout (which we send with invalid config) is set extra high since it usually means the user/pass is wrong.
	constexpr static auto tdsb_grabber = grabber::make_https_grabber(init, loop, pdMS_TO_TICKS(60*10*1000), pdMS_TO_TICKS(30*1000));
}

#endif
