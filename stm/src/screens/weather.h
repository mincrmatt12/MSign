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

		bool interact();
	private:
		void draw_currentstats();
		void draw_hourlybar_header();
		void draw_hourlybar(uint8_t hour);
		void draw_status();
		void draw_graph(int axisx=79, int axisy=24);

		enum Subscreen : uint8_t {
			MAIN,
			BIG_GRAPH,
			BIG_HOURLY,

			MAX_SUBSCREEN
		} subscreen=MAIN, highlight=BIG_GRAPH;
	};
}

#endif

