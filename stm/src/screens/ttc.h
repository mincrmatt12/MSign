#ifndef TTC_H
#define TTC_H

#include "base.h"
#include <stdint.h>

namespace screen {
	struct TTCScreen : public Screen {
		TTCScreen();
		~TTCScreen();

		static void prepare(bool);

		void draw();
	private:
		void draw_bus();
		bool draw_slot(uint16_t y, const uint8_t * name, const uint64_t times[6], bool alert, bool delay);
		void draw_alertstr();

		uint8_t bus_type = 1;
		uint8_t bus_state = 0;
	};
}

#endif
