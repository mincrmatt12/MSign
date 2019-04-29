#ifndef TTC_H
#define TTC_H

#include "schedef.h"
#include <stdint.h>

namespace tasks {
	struct TTCScreen : public sched::Task, sched::Screen {
		bool init() override;
		bool deinit() override;

		void loop() override;
		bool done() override {return true;}
	private:
		void draw_bus();
		bool draw_slot(uint16_t y, const uint8_t * name, uint64_t time1, uint64_t time2, bool alert, bool delay);

		uint8_t s_info = 0xff;
		uint8_t s_t[3] = {0xff};
		uint8_t s_n[3] = {0xff};

		bool ready;

		uint8_t bus_type = 1;
	};
}

#endif
