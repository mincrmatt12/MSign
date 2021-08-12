#ifndef WEATHER_H
#define WEATHER_H

#include "base.h"
#include <stdint.h>
#include "../common/slots.h"

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

		void draw_graph_yaxis(int16_t x, int16_t y0, int16_t y1, int32_t &ymin, int32_t &ymax, bool show_decimal=false);
		void draw_graph_xaxis(int16_t y, int16_t x0, int16_t y1, int min, bool interpret_as_hours=true);

		void draw_graph_lines(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const int16_t * data, size_t amount, int32_t ymin, int32_t ymax, bool show_temp_colors=false);
		void draw_graph_precip(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const slots::PrecipData * data, size_t amount, int32_t ymin, int32_t ymax);

		void draw_small_tempgraph();
		void draw_big_graphs();

		enum Subscreen : uint8_t {
			MAIN,
			BIG_GRAPH,
			BIG_HOURLY,

			MAX_SUBSCREEN
		} subscreen=MAIN, highlight=BIG_GRAPH;

		enum GraphType : uint8_t {
			FEELS_TEMP,
			REAL_TEMP,

			WIND,

			PRECIP_HOUR,
			PRECIP_DAY,

			MAX_GTYPE
		} graph = FEELS_TEMP;
		TickType_t show_graph_selector_timer = 0;
	};
}

#endif

