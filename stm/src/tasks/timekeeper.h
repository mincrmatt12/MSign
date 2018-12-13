#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include "sched.h"
#include <stdint.h>

namespace tasks {
	struct Timekeeper : public sched::Task {
		Timekeeper(uint64_t &timestamp);

		bool important() override;
		bool done() override;
		void loop() override;

		void systick_interrupt();
	private:
		uint32_t last_run_time = 0;
	};
}

#endif
