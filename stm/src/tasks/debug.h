#ifndef MSS_DEBUG_H
#define MSS_DEBUG_H

#include <stdint.h>
#include "timekeeper.h"

namespace tasks {
	struct DebugConsole {
		DebugConsole(Timekeeper &tim) : tim(tim) {
		}

		void run();

	private:
		Timekeeper& tim;
	};
}

#endif
