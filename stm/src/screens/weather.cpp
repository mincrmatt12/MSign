#include "weather.h"
#include "../srv.h"
#include "../tasks/timekeeper.h"
#include "../common/slots.h"
#include "../fonts/latob_15.h"
#include "../fonts/dejavu_10.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/lcdpixel_6.h"
#include "../intmath.h"

#include "../draw.h"
#include <time.h>

#include <cmath>
extern uint64_t rtc_time;
extern tasks::Timekeeper timekeeper;
extern matrix_type matrix;
extern srv::Servicer servicer;

namespace bitmap::weather {
// w=20, h=20, stride=3, color=202, 252, 17
const uint8_t clear_day[] = {
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b01000000,0b00000000,
        0b00100000,0b01000000,0b10000000,
        0b00010000,0b01000001,0b00000000,
        0b00001000,0b00000010,0b00000000,
        0b00000001,0b11110000,0b00000000,
        0b00000010,0b00001000,0b00000000,
        0b00000100,0b00000100,0b00000000,
        0b00000100,0b00000100,0b00000000,
        0b01110100,0b00000101,0b11000000,
        0b00000100,0b00000100,0b00000000,
        0b00000100,0b00000100,0b00000000,
        0b00000010,0b00001000,0b00000000,
        0b00000001,0b11110000,0b00000000,
        0b00001000,0b00000010,0b00000000,
        0b00010000,0b01000001,0b00000000,
        0b00100000,0b01000000,0b10000000,
        0b00000000,0b01000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000
};

// w=20, h=20, stride=3, color=118, 118, 118
const uint8_t fog[] = {
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00011110,0b01110011,0b10000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00011111,0b11111111,0b10000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00011111,0b11111111,0b10000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00011111,0b11111111,0b10000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000
};

// w=20, h=20, stride=3, color=79, 78, 79
const uint8_t night[] = {
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00111000,0b00000000,
        0b00000000,0b11000100,0b00000000,
        0b00000001,0b00001000,0b00000000,
        0b00000010,0b00010000,0b00000000,
        0b00000010,0b00100000,0b00000000,
        0b00000010,0b00100000,0b00000000,
        0b00000100,0b01000000,0b00000000,
        0b00000100,0b01000000,0b00000000,
        0b00000100,0b01000000,0b00000000,
        0b00000100,0b01000000,0b00000000,
        0b00000100,0b01000000,0b00000000,
        0b00000010,0b00100000,0b00000000,
        0b00000010,0b00100000,0b00000000,
        0b00000010,0b00010000,0b00000000,
        0b00000001,0b00001000,0b00000000,
        0b00000000,0b11000100,0b00000000,
        0b00000000,0b00111000,0b00000000,
        0b00000000,0b00000000,0b00000000
};

// w=20, h=20, stride=3, color=235, 235, 235
const uint8_t cloudy[] = {
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000111,0b10000000,0b00000000,
        0b00011000,0b01100000,0b00000000,
        0b00100000,0b00010000,0b00000000,
        0b01000000,0b00001000,0b00000000,
        0b01000000,0b00001011,0b11000000,
        0b01000000,0b00000100,0b00100000,
        0b01000000,0b00001000,0b00100000,
        0b01000000,0b00000000,0b00100000,
        0b01000000,0b00000000,0b00100000,
        0b00100000,0b00000000,0b01000000,
        0b00011111,0b11111111,0b10000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000
};

// w=20, h=20, stride=3, color=43, 182, 255
const uint8_t rain[] = {
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000111,0b10000000,0b00000000,
        0b00011000,0b01100000,0b00000000,
        0b00100000,0b00010000,0b00000000,
        0b01000000,0b00001000,0b00000000,
        0b01000000,0b00001011,0b11000000,
        0b01000000,0b00000100,0b00100000,
        0b01000000,0b00001000,0b00100000,
        0b01000000,0b00000000,0b00100000,
        0b01000000,0b00000000,0b00100000,
        0b00100010,0b01001000,0b01000000,
        0b00011010,0b01001011,0b10000000,
        0b00000010,0b01001000,0b00000000,
        0b00000010,0b01001000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000
};

// w=20, h=20, stride=3, color=255, 255, 255
const uint8_t snow[] = {
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000111,0b10000000,0b00000000,
        0b00011000,0b01100000,0b00000000,
        0b00100000,0b00010000,0b00000000,
        0b01000000,0b00001000,0b00000000,
        0b01000000,0b00001011,0b11000000,
        0b01000000,0b00000100,0b00100000,
        0b01000000,0b00001000,0b00100000,
        0b01000000,0b00000000,0b00100000,
        0b01000001,0b01010000,0b00100000,
        0b00100000,0b10100000,0b01000000,
        0b00011001,0b00010011,0b10000000,
        0b00000000,0b10100000,0b00000000,
        0b00000001,0b01010000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000
};

// w=20, h=20, stride=3, color=118, 118, 118
const uint8_t wind[] = {
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00011011,0b00000000,
        0b00000000,0b00100100,0b10000000,
        0b00000000,0b00000010,0b10000000,
        0b00000000,0b00000010,0b10000000,
        0b00000000,0b00000101,0b00000000,
        0b00000000,0b00011000,0b00000000,
        0b00111111,0b11100000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000,
        0b00000000,0b00000000,0b00000000
};
}

void screen::WeatherScreen::prepare(bool) {
	servicer.set_temperature_all<
		slots::WEATHER_ARRAY,
		slots::WEATHER_ICON,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_TIME_SUN
	>(bheap::Block::TemperatureWarm);
}

screen::WeatherScreen::WeatherScreen() {
	servicer.set_temperature_all<
		slots::WEATHER_ARRAY,
		slots::WEATHER_ICON,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_TIME_SUN
	>(bheap::Block::TemperatureHot);
}

screen::WeatherScreen::~WeatherScreen() {
	servicer.set_temperature_all<
		slots::WEATHER_ARRAY,
		slots::WEATHER_ICON,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_TIME_SUN
	>(bheap::Block::TemperatureCold);
}

void screen::WeatherScreen::draw_currentstats() {
	char disp_buf[16];
	auto ctemp = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->ctemp;
	{
		auto v = intmath::round10(ctemp, (int16_t)10);
		snprintf(disp_buf, 16, "%d.%d", v / 10, std::abs(v % 10));
	}

	uint16_t text_size;

	text_size = draw::text_size(disp_buf, font::lato_bold_15::info);

	if (ctemp > 1000 && ctemp < 1600) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 240_c);
	}
	else if (ctemp < 1000 && ctemp > -1000) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 100_c);
	}
	else if (ctemp >= 1000 && ctemp < 3000) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, {50_c, 240_c, 50_c});
	}
	else if (ctemp >= 3000) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, {250_c, 30_c, 30_c});
	}
	else {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, {40_c, 40_c, 255_c});
	}

	snprintf(disp_buf, 16, "%d", intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->ltemp));
	draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, 19, 30, "\xfe ", 0x7f7ff0_cc, disp_buf, 127_c);

	snprintf(disp_buf, 16, "\xfd %d", intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->htemp));
	text_size = draw::text_size(disp_buf, font::dejavusans_10::info);
	snprintf(disp_buf, 16, "%d", intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->htemp));
	draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, 69-text_size, 30, "\xfd ", led::color_t{255_c, 127_c, 10_c}, disp_buf, led::color_t{127_c, 127_c, 127_c});

	{
		auto v = intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->crtemp, (int16_t)10);
		snprintf(disp_buf, 16, "%d.%d", v / 10, std::abs(v % 10));
		text_size = draw::text_size(disp_buf, font::lcdpixel_6::info);
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lcdpixel_6::info, 44 - text_size / 2, 20, 127_c);
	}

	int16_t y = (int16_t)(std::round(2.5f * sinf((float)(timekeeper.current_time) / 700.0f) + 3));

	const char * icon = (char*)*servicer[slots::WEATHER_ICON];

	if (strcmp(icon, "cloudy") == 0 || strcmp(icon, "partly-cloudy") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::cloudy, 20, 20, 3, 1, y, 235_c);
	}
	else if (strcmp(icon, "sleet") == 0 || strcmp(icon, "snow") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::snow, 20, 20, 3, 1, y, 255_c);
	}
	else if (strcmp(icon, "clear-day") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::clear_day, 20, 20, 3, 1, y, {220_c, 250_c, 0_c});
	}
	else if (strcmp(icon, "clear-night") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::night, 20, 20, 3, 1, y, {79_c, 78_c, 79_c});
	}
	else if (strcmp(icon, "wind") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::wind, 20, 20, 3, 1, y, {118_c, 118_c, 118_c});
	}
	else if (strcmp(icon, "fog") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::fog, 20, 20, 3, 1, y, {118_c, 118_c, 118_c});
	}
	else if (strcmp(icon, "rain") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::rain, 20, 20, 3, 1, y, {43_c, 182_c, 255_c});
	}
	else {
		if (icon[15] != 0 && icon[14] == 'd') {
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::cloudy, 20, 20, 3, 1, y, 245_c);
		}
		else if (icon[15] != 0) {
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::cloudy, 20, 20, 3, 1, y, 79_c);
		}
	}
}

void screen::WeatherScreen::draw_status() {
	if (servicer.slot(slots::WEATHER_STATUS)) {
		uint16_t text_size = draw::text_size(*servicer[slots::WEATHER_STATUS], font::tahoma_9::info);
		if (text_size < 128) {
			draw::text(matrix.get_inactive_buffer(), *servicer[slots::WEATHER_STATUS], font::tahoma_9::info, 64 - text_size / 2, 61, 240_c);
		}
		else {
			int16_t t_pos = draw::scroll(timekeeper.current_time / std::min<uint64_t>(10, std::max<uint64_t>(5, 40 - (strlen((char*)*servicer[slots::WEATHER_STATUS]) / 7))), text_size);
			draw::text(matrix.get_inactive_buffer(), *servicer[slots::WEATHER_STATUS], font::tahoma_9::info, t_pos, 61, 240_c);
		}
	}
}

void screen::WeatherScreen::draw_hourlybar_header() {
	// bar starts at x = 4, ends at x = 124, every 5 steps = 1 thing
	// bar takes 13 px vertically, starts at y = 53
	
	struct tm timedat;
	time_t now = rtc_time / 1000;
	gmtime_r(&now, &timedat);
	int first = timedat.tm_hour;

	for (int i = 0; i < 24 / 4; ++i) {
		int hour = (first + i * 4) % 24;
		char buf[3] = {0};
		snprintf(buf, 3, "%02d", hour);

		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, 4 + i * 20, 38, 255_c);
	}
	draw::multi_gradient_rect(matrix.get_inactive_buffer(), 4, 39, 40, 30_cu, 64, 0x384238_ccu, 124, 30_cu);
	draw::multi_gradient_rect(matrix.get_inactive_buffer(), 4, 51, 52, 30_cu, 64, 0x384238_ccu, 124, 30_cu);
}

void screen::WeatherScreen::draw_hourlybar(uint8_t hour) {
	int start = 4 + hour * 5;
	int end =   9 + hour * 5;

	led::color_t col(127), hatch(127);
	bool do_hatch = false;
    slots::WeatherStateArrayCode code = (*servicer.slot<slots::WeatherStateArrayCode *>(slots::WEATHER_ARRAY))[hour]; 
	
	struct tm timedat;
	time_t now = rtc_time / 1000;
	gmtime_r(&now, &timedat);
	hour = (timedat.tm_hour + hour) % 24;
	int64_t hourstart = (int64_t)hour * (1000*60*60);

	const auto &times = *servicer.slot<slots::WeatherTimes>(slots::WEATHER_TIME_SUN);

	switch (code) {
		case slots::WeatherStateArrayCode::CLEAR:
			if (hourstart > times.sunrise && hourstart < times.sunset) {
				col.r = 210_c;
				col.g = 200_c;
				col.b = 0_c;
			}
			else {
				col = led::color_t{0};
			}
			break;
		
		case slots::WeatherStateArrayCode::PARTLY_CLOUDY:
			col = led::color_t(130); break;
		case slots::WeatherStateArrayCode::MOSTLY_CLOUDY:
			col = led::color_t(80); break;
		case slots::WeatherStateArrayCode::OVERCAST:
			col = led::color_t(50); break;

		case slots::WeatherStateArrayCode::SNOW:
			col = led::color_t(200);
			hatch = led::color_t(80);
			do_hatch = true;
		    break;
		case slots::WeatherStateArrayCode::HEAVY_SNOW:
			col = led::color_t(255);
			hatch = led::color_t(40);
			do_hatch = true;
			break;
		case slots::WeatherStateArrayCode::DRIZZLE:
			col.r = col.g = 30_c;
			col.b = 90_c;
			break;
		case slots::WeatherStateArrayCode::LIGHT_RAIN:
			col.r = col.g = 30_c;
			col.b = 150_c;
			break;
		case slots::WeatherStateArrayCode::RAIN:
			col.r = col.g = 55_c;
			col.b = 220_c;
			break;
		case slots::WeatherStateArrayCode::HEAVY_RAIN:
			col.r = col.g = 60_c;
			col.b = 255_c;
			hatch.r = hatch.g = 50_c;
			hatch.b = 100_c;
			do_hatch = true;
			break;
		default:
			break;
	}

	if (do_hatch) 
		draw::hatched_rect(matrix.get_inactive_buffer(), start, 40, end, 51, col, hatch);
	else
		draw::rect(matrix.get_inactive_buffer(), start, 40, end, 51, col);
}

void screen::WeatherScreen::draw_graph(int left_, int bottom_) {
	// graph axes on pos 79 x, 24 y
	
	draw::rect(matrix.get_inactive_buffer(), left_, bottom_, 128, bottom_ + 1, 50_c);
	draw::rect(matrix.get_inactive_buffer(), left_, 0, left_ + 1, bottom_ + 1, 50_c);

	const int16_t * tempgraph_data = *servicer.slot<int16_t *>(slots::WEATHER_TEMP_GRAPH);
	const int32_t space = bottom_;

	int32_t min_ = INT16_MAX, max_ = INT16_MIN;
	for (uint8_t i = 0; i < 24; ++i) {
		min_ = std::min<int32_t>(min_, tempgraph_data[i]);
		max_ = std::max<int32_t>(max_, tempgraph_data[i]);
	}

	// The actual min/max is rounded down/up +/- 1
	min_ = intmath::floor10(min_)*100 - 100;
	max_ = intmath::ceil10(max_)*100 + 100;

	auto vert_mark = [left_](int16_t y, int32_t v){
		matrix.get_inactive_buffer().at(left_ - 1, y) = led::color_t(95_c);

		char buf[10] = {0};
		snprintf(buf, 10, "%d", intmath::round10(v));

		int16_t sz = draw::text_size(buf, font::lcdpixel_6::info);
		int16_t x = left_ - sz - 2;
		y += 3;
		if (y < 5) y = 5;

		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y, 120_c);
	};

	auto hori_mark = [bottom_, left_](int16_t x, int hr) {
		matrix.get_inactive_buffer().at(x, bottom_ + 1) = led::color_t(95_c);

		char buf[10] = {0};
		snprintf(buf, 10, "%02d", hr);

		if (left_ < 64) {
			draw::multi_text(matrix.get_inactive_buffer(), font::lcdpixel_6::info, x, bottom_ + 7, buf, 0x9dd3e3_cc, ":00", 120_c);
		}
		else {
			draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, bottom_ + 7, 120_c);
		}
	};

	// based on min/max, figure out minimum offset:
	
	int32_t tickmark_min = intmath::ceil10(6 * (max_ - min_) / space);

	for (int i = 0;; ++i) {
		int32_t value = (tickmark_min * i) * 100;
		int32_t pos = intmath::round10((space*100 - 100) - ((space * value * 100) / (max_ - min_)));

		if (pos < 0) break;

		vert_mark(pos, value + min_);
	}

	struct tm timedat;
	time_t now = rtc_time / 1000;
	gmtime_r(&now, &timedat);
	int first = timedat.tm_hour;


	if (left_ < 64) {
		for (int i = 0; i < 6; ++i) {
			int pos = left_ + 1 + (i*(128 - left_ - 1)) / 6;
			hori_mark(pos, (first + i * 4) % 24);
		}
	}
	else {
		for (int i = 0; i < 6; ++i) {
			int pos = left_ + 1 + (i*(128 - left_ - 1)) / 4;
			hori_mark(pos, (first + i * 6) % 24);
		}
	}

	for (int hour = 0; hour < 24; ++hour) {
		int begin = hour * (128 - left_ - 1) / 24;
		int end = (hour + 1) * (128 - left_ - 1) / 24;
		begin += left_ + 1;
		end += left_ + 1;

		int16_t y0 = intmath::round10((int32_t(tempgraph_data[hour] - min_) * space * 100) / (max_ - min_));
		int16_t y1 = (hour == 23 ? y0 : intmath::round10((int32_t(tempgraph_data[hour + 1] - min_) * space * 100) / (max_ - min_)));

		y0 = bottom_ - 1 - y0;
		y1 = bottom_ - 1 - y1;
		if (hour == 23) y1 = y0;

		auto ctemp = tempgraph_data[hour];
		if (ctemp >= 1000 && ctemp < 1600) {
			draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, 255_c);
		}
		else if (ctemp > -1000 && ctemp < 1000) {
			draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, {100_c, 100_c, 240_c});
		}
		else if (ctemp >= 1600 && ctemp < 3000) {
			draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, {50_c, 240_c, 50_c});
		}
		else if (ctemp >= 3000) {
			draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, {250_c, 30_c, 30_c});
		}
		else {
			draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, {40_c, 40_c, 255_c});
		}
	}
}

void screen::WeatherScreen::draw() {
	// Lock the contents
	srv::ServicerLockGuard g(servicer);

	switch (subscreen) {
	default:
		if (servicer.slot(slots::WEATHER_STATUS) && servicer.slot(slots::WEATHER_ICON)) {
			draw_currentstats();
			draw_status();
			draw_hourlybar_header();
			if (servicer.slot(slots::WEATHER_ARRAY) && servicer.slot(slots::WEATHER_TIME_SUN)) {
				for (int i = 0; i < 24; ++i) {
					draw_hourlybar(i);
				}
			}
			if (servicer.slot(slots::WEATHER_TEMP_GRAPH))  {
				draw_graph();
			}
		}
		break;
	case BIG_GRAPH:
		// TODO: more info, for now just draw the graph enlarged
		if (!servicer.slot(slots::WEATHER_TEMP_GRAPH)) {
			// show loading spinner (TODO)
			draw::text(matrix.get_inactive_buffer(), "loading", font::dejavusans_10::info, 50, 34, 0xff_c);
		}
		else {
			draw_graph(14, 64-8);
		}
		break;
	}
}

bool screen::WeatherScreen::interact() {
	switch (subscreen) {
		case MAIN:
			// draw outline
			switch (highlight) {
				default: break;
				case BIG_GRAPH: 
					draw::dashed_outline(matrix.get_inactive_buffer(), 79, 0, 127, 24, 0xff_c);
					break;
				case BIG_HOURLY:
					draw::dashed_outline(matrix.get_inactive_buffer(), 2, 31, 125, 53, 0xff_c);
					break;
			}

			if (ui::buttons[ui::Buttons::NXT]) {
				highlight = (Subscreen)((int)highlight + 1);
				if (highlight == MAX_SUBSCREEN) highlight = BIG_GRAPH;
			}
			else if (ui::buttons[ui::Buttons::PRV]) {
				highlight = (Subscreen)((int)highlight - 1);
				if (highlight == MAIN) highlight = BIG_HOURLY;
			}
			else if (ui::buttons[ui::Buttons::SEL]) {
				subscreen = highlight;
			}
			else if (ui::buttons[ui::Buttons::POWER]) return true;
			break;
		default:
			if (ui::buttons[ui::Buttons::POWER]) subscreen = MAIN;
			break;
	}
	return false;
}
