#ifndef WEATHER_H
#define WEATHER_H

#include "schedef.h"
#include "../vstr.h"
#include <stdint.h>

namespace tasks {
	struct WeatherScreen : public sched::Task, sched::Screen {
		
		bool init() override;
		bool deinit() override;

		bool done() override {return true;}
		void loop() override;

	private:
		
		srv::vstr::VSWrapper s_status;
		uint8_t s_info, s_icon;

	};
}

#endif

