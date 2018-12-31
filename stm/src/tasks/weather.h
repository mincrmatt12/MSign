#ifndef WEATHER_H
#define WEATHER_H

#include "sched.h"
#include <stdint.h>

namespace tasks {
	struct WeatherScreen : public sched::Task, sched::Screen {
		
		bool init() override;
		bool deinit() override;

		bool done() override {return true;}
		void loop() override;

	private:

		void update_status();
		
		char status[128];
		uint8_t status_update_state = 0;

		uint8_t s_info, s_status, s_icon;

	};
}

#endif

