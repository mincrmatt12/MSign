#ifndef DISPMAN_H
#define DISPMAN_H

#include "schedef.h"
#include "tasks/clock.h"
#include "tasks/ttc.h"
#include "tasks/weather.h"
#include "tasks/threed.h"
#include "tasks/calfix.h"

namespace tasks {
	struct DispMan : public sched::Task {
		bool done() override {return true;}
		
		void loop() override;
		void init();

		// button interface
		void set_hold(bool keep_on_screen);
		void force_advance();
	private:
		void communicate();
		uint8_t next(uint8_t i);

		void setup(uint8_t i);
		void teardown(uint8_t i, bool call_deinit=true);
		void activate(uint8_t i);

		TTCScreen ttc;
		WeatherScreen weather;
		ClockScreen clockfg;
		threed::Renderer threedbg;
		CalfixScreen calfix;

		uint8_t on;
		uint64_t last_screen_transition = 0;
		const static uint8_t count = 4;

		uint64_t times_on[count] = {12000, 12000, 12000, 12000};

		bool data_ok = true;
		uint16_t enabled_mask = (1 << count) - 1;

		uint8_t slot_cfg, slot_time;
		uint8_t counter = 0;

		bool is_holding = false, should_advance = false;
	};
}

#endif
