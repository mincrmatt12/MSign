#ifndef MSIGN_SC_H
#define MSIGN_SC_H

#include "common/slots.h"

namespace sccfg {
	void init();
	void loop();

	void disable_screen(slots::ScCfgInfo::EnabledMask e);
	void enable_screen(slots::ScCfgInfo::EnabledMask e);
}

#endif
