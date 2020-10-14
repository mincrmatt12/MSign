#ifndef WEATHER_H
#define WEATHER_H

#include "base.h"
#include <stdint.h>

namespace screen {
	struct WeatherScreen : public Screen {
		WeatherScreen();
		~WeatherScreen();

		static void prepare(bool);

		void draw();

	private:
		void draw_currentstats();
		void draw_hourlybar_header();
		void draw_hourlybar(uint8_t hour);
		void draw_status();
		void draw_graphaxes();
		void draw_graph(uint8_t hour);
	};
}

#endif

