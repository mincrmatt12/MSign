#ifndef DISPMAN_H
#define DISPMAN_H

#include "sched.h"
#include "tasks/clock.h"
#include "tasks/ttc.h"
#include "tasks/weather.h"
#include "tasks/threed.h"

namespace tasks {
	struct DispMan : public sched::Task {
		bool done() override {return true;}
		
		void loop() override;
		void init();
	private:
		void setup(uint8_t i);
		void teardown(uint8_t i);
		void activate(uint8_t i);

		TTCScreen ttc;
		WeatherScreen weather;
		ClockScreen clockfg;
		threed::Renderer threedbg;

		uint8_t on;
		uint64_t last_screen_transition = 0;
		const static uint8_t count = 3;
	};
}

#endif
