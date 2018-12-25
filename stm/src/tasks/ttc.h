#ifndef TTC_H
#define TTC_H

#include "sched.h"
#include <stdint.h>

namespace tasks {
	struct TTCScreen : public sched::Task, sched::Screen {
		void init() override {};
		void deinit() override {};

		void loop() override;
		bool done() override {return true;}
	private:
		void draw_bus();
		bool draw_slot(uint16_t y, const char * name, uint64_t time1, uint64_t time2);
	};
}

#endif
