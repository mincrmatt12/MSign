#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include "schedef.h"
#include <stdint.h>

namespace tasks {
	struct Timekeeper : public sched::Task {
		Timekeeper(uint64_t &timestamp) :
			timestamp(timestamp) {
				name[0] = 't';
				name[1] = 'i';
				name[2] = 'm';
				name[3] = 'e';
		};

		bool important() override;
		bool done() override;
		void loop() override;

		void systick_handler();

		// Current time; as in the systick time. Useful if you don't want the clock going backwards when the ESP updates / animation.
		uint32_t current_time =  0;
	private:
		uint32_t last_run_time = 0;
		uint32_t ack_timer = 0;
		bool waiting_for_data = false;

		uint64_t &timestamp;
		uint8_t   time_slot;
	};
}

#endif
