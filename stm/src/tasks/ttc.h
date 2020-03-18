#ifndef TTC_H
#define TTC_H

#include "schedef.h"
#include "vstr.h"
#include <stdint.h>

namespace tasks {
	struct TTCScreen : public sched::Task, sched::Screen {
		bool init() override;
		bool deinit() override;

		void loop() override;
		bool done() override {return true;}
	private:
		void draw_bus();
		bool draw_slot(uint16_t y, const uint8_t * name, uint64_t times[4], bool alert, bool delay);
		void draw_alertstr();

		uint8_t s_info = 0xff;
		uint8_t s_t[3] = {0xff};
		uint8_t s_tb[3] = {0xff};
		uint8_t s_n[3] = {0xff};
		srv::vstr::SizeVSWrapper<char, 256> s_alert;

		bool ready;

		uint8_t bus_type = 1;
		uint8_t bus_state = 0;
	};
}

#endif
