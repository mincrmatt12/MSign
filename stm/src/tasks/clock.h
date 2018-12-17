#ifndef CLOCK_S_H
#define CLOCK_S_H

#include "sched.h"
#include <stdint.h>

namespace tasks {
	struct ClockScreen : public sched::Task, public sched::Screen {
		void init() override;

		void loop() override;
		bool done() override {return true;}
	private:
		uint8_t bg_color[3];
	};
}

#endif
