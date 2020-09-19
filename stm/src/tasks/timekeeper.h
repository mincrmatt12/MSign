#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <stdint.h>

namespace tasks {
	struct Timekeeper {
		Timekeeper(uint64_t &timestamp) :
			timestamp(timestamp) {
		}

		void loop();
		void systick_handler();

		// Current time; as in the systick time. Useful if you don't want the clock going backwards when the ESP updates / animation.
		uint32_t current_time =  0;
	private:
		uint32_t last_run_time = 0;
		bool first = true;

		uint64_t &timestamp;
	};
}

#endif
