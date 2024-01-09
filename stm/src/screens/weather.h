#ifndef WEATHER_H
#define WEATHER_H

#include "base.h"
#include <stdint.h>
#include "../common/slots.h"
#include "../common/bheap.h"

namespace screen {
	struct WeatherScreen : public Screen {
		WeatherScreen();
		~WeatherScreen();

		static void prepare(bool);

		void draw();
		void refresh();

		bool interact();
	private:
		void draw_currentstats();
		void draw_hourlybar_header();
		void draw_hourlybar(uint8_t hour);
		void draw_status();

		void fill_hourlybar(int16_t x0, int16_t y0, int16_t x1, int16_t y1, slots::WeatherStateCode wsac, const char *& desc_out, int64_t hourstart, bool vertical_sunrise=false);

		void draw_graph_yaxis(int16_t x, int16_t y0, int16_t y1, int32_t &ymin, int32_t &ymax, bool show_decimal=false);
		// Draws the x-axis for the mini-graph window
		void draw_graph_xaxis(int16_t y, int16_t x0, int16_t x1, int min, bool interpret_as_hours=true);
		// Draws the x-axis for the full graph window. Uses two lines (needs 2 + 6 + 6 = 14 pixels y-padding).
		//
		// Assumes the window x0-x1 is 1hr for minutely and 1d for hourly. 
		void draw_graph_xaxis_full(int16_t y, int16_t x0, int16_t x1, int x_scroll, int maxscroll,  bool range_hours=false);

		void draw_graph_lines(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const int16_t * data, int per_screen, int total_amount, int32_t ymin, int32_t ymax, bool show_temp_colors=false, int x_scroll=0);
		void draw_graph_precip(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const bheap::TypedBlock<slots::PrecipData *>& precip_data, int per_screen, int desired_amount, int index_offset, int32_t ymin, int32_t ymax, int x_scroll=0);

		void draw_small_tempgraph();
		void draw_small_precgraph();
		void draw_big_graphs();

		void draw_big_hourlybar();

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
			GUST,

			PRECIP_HOUR,
			PRECIP_DAY,

			MAX_GTYPE
		} graph = FEELS_TEMP;
		enum GraphScrollType : uint8_t {
			NO_SCROLL,
			SCROLL_WINDOW,
			SCROLL_CURSOR
		} graph_scroll_type = NO_SCROLL;
		TickType_t show_graph_selector_timer = 0;
		int expanded_hrbar_scroll = 0;
		int expanded_graph_scroll = 0;
	};
}

#endif

