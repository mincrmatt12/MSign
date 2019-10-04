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
		void draw_currentstats();
		void draw_hourlybar_header();
		void draw_hourlybar(uint8_t hour);
		void draw_status();
		
		srv::vstr::VSWrapper s_status;
		srv::vstr::SizeVSWrapper<float, 192> s_tempgraph;
		uint8_t s_info, s_icon, s_state[2];
	};
}

#endif

