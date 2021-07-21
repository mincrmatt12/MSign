#ifndef CLOCK_S_H
#define CLOCK_S_H

#include "base.h"
#include "../matrix.h"
#include <stdint.h>

namespace screen {
	struct ClockScreen : public Screen {
		ClockScreen();

		void draw();
	private:
		led::color_t bg_color;
	};
}

#endif
