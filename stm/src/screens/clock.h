#ifndef CLOCK_S_H
#define CLOCK_S_H

#include "base.h"
#include <stdint.h>

namespace screen {
	struct ClockScreen : public Screen {
		ClockScreen();

		void draw();
	private:
		uint16_t bg_color[3];
	};
}

#endif
