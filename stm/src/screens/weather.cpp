#include "weather.h"
#include "../srv.h"
#include "../tasks/timekeeper.h"
#include "../common/slots.h"
#include "../fonts/latob_15.h"
#include "../fonts/dejavu_10.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/lcdpixel_6.h"

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
	servicer.set_temperature_all(bheap::Block::TemperatureWarm,
		slots::WEATHER_ARRAY,
		slots::WEATHER_ICON,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_TIME_SUN
	);
}

screen::WeatherScreen::WeatherScreen() {
	servicer.set_temperature_all(bheap::Block::TemperatureHot,
		slots::WEATHER_ARRAY,
		slots::WEATHER_ICON,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_TIME_SUN
	);
}

screen::WeatherScreen::~WeatherScreen() {
	servicer.set_temperature_all(bheap::Block::TemperatureCold,
		slots::WEATHER_ARRAY,
		slots::WEATHER_ICON,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_TIME_SUN
	);
}

void screen::WeatherScreen::draw_currentstats() {
	char disp_buf[16];
	float ctemp = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->ctemp;
	snprintf(disp_buf, 16, "%.01f", ctemp);

	uint16_t text_size;

	text_size = draw::text_size(disp_buf, font::lato_bold_15::info);

	if (ctemp > 10.0 && ctemp < 16.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 240_c, 240_c, 240_c);
	}
	else if (ctemp < 10.0 && ctemp > -10.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 100_c, 100_c, 240_c);
	}
	else if (ctemp >= 16.0 && ctemp < 30.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 50_c, 240_c, 50_c);
	}
	else if (ctemp > 30.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 250_c, 30_c, 30_c);
	}
	else {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 40_c, 40_c, 255_c);
	}

	snprintf(disp_buf, 16, "%.0f", servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->ltemp);
	draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, 19, 30, "\xfe ", 127_c, 127_c, 240_c, disp_buf, 127_c, 127_c, 127_c);

	snprintf(disp_buf, 16, "\xfd %.0f", servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->htemp);
	text_size = draw::text_size(disp_buf, font::dejavusans_10::info);
	snprintf(disp_buf, 16, "%.0f", servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->htemp);
	draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, 69-text_size, 30, "\xfd ", 255_c, 127_c, 10_c, disp_buf, 127_c, 127_c, 127_c);

	snprintf(disp_buf, 16, "%.01f", servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->crtemp);
	text_size = draw::text_size(disp_buf, font::lcdpixel_6::info);
	draw::text(matrix.get_inactive_buffer(), disp_buf, font::lcdpixel_6::info, 44 - text_size / 2, 20, 127_c, 127_c, 127_c);

	int16_t y = (int16_t)(std::round(2.5f * sinf((float)(timekeeper.current_time) / 700.0f) + 3));

	const char * icon = (char*)*servicer[slots::WEATHER_ICON];

	if (strcmp(icon, "cloudy") == 0 || strcmp(icon, "partly-cloudy") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::cloudy, 20, 20, 3, 1, y, 235_c, 235_c, 235_c);
	}
	else if (strcmp(icon, "sleet") == 0 || strcmp(icon, "snow") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::snow, 20, 20, 3, 1, y, 255_c, 255_c, 255_c);
	}
	else if (strcmp(icon, "clear-day") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::clear_day, 20, 20, 3, 1, y, 220_c, 250_c, 0_c);
	}
	else if (strcmp(icon, "clear-night") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::night, 20, 20, 3, 1, y, 79_c, 78_c, 79_c);
	}
	else if (strcmp(icon, "wind") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::wind, 20, 20, 3, 1, y, 118_c, 118_c, 118_c);
	}
	else if (strcmp(icon, "fog") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::fog, 20, 20, 3, 1, y, 118_c, 118_c, 118_c);
	}
	else if (strcmp(icon, "rain") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::rain, 20, 20, 3, 1, y, 43_c, 182_c, 255_c);
	}
	else {
		if (icon[15] != 0 && icon[14] == 'd') {
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::cloudy, 20, 20, 3, 1, y, 245_c, 245_c, 245_c);
		}
		else if (icon[15] != 0) {
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::cloudy, 20, 20, 3, 1, y, 79_c, 79_c, 79_c);
		}
	}
}

void screen::WeatherScreen::draw_status() {
	if (servicer.slot(slots::WEATHER_STATUS)) {
		uint16_t text_size = draw::text_size(*servicer[slots::WEATHER_STATUS], font::tahoma_9::info);
		if (text_size < 128) {
			draw::text(matrix.get_inactive_buffer(), *servicer[slots::WEATHER_STATUS], font::tahoma_9::info, 64 - text_size / 2, 61, 240_c, 240_c, 240_c);
		}
		else {
			int16_t t_pos = draw::scroll(timekeeper.current_time / std::min<uint64_t>(10, std::max<uint64_t>(5, 40 - (strlen((char*)*servicer[slots::WEATHER_STATUS]) / 7))), text_size);
			draw::text(matrix.get_inactive_buffer(), *servicer[slots::WEATHER_STATUS], font::tahoma_9::info, t_pos, 61, 240_c, 240_c, 240_c);
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

		// todo  make this a gradient
		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, 3 + i * 20, 38, 255_c, 255_c, 255_c);
	}
	draw::rect(matrix.get_inactive_buffer(), 4, 39, 124, 40, 50_c, 50_c, 50_c);
	draw::rect(matrix.get_inactive_buffer(), 4, 51, 124, 52, 50_c, 50_c, 50_c);
}

void screen::WeatherScreen::draw_hourlybar(uint8_t hour) {
	int start = 4 + hour * 5;
	int end =   9 + hour * 5;

	uint16_t r = 127, g = 127, b = 127;
	uint16_t hatch_r = 127, hatch_g = 127, hatch_b = 127;
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
				r = 210_c;
				g = 200_c;
				b = 0_c;
			}
			else {
				r = g = b = 0_c;
			}
			break;
		
		case slots::WeatherStateArrayCode::PARTLY_CLOUDY:
			r = g = b = 130_c; break;
		case slots::WeatherStateArrayCode::MOSTLY_CLOUDY:
			r = g = b = 80_c; break;
		case slots::WeatherStateArrayCode::OVERCAST:
			r = g = b = 50_c; break;

		case slots::WeatherStateArrayCode::SNOW:
			r = g = b = 200_c;
			hatch_r = hatch_g = hatch_b = 80_c;
			do_hatch = true;
		    break;
		case slots::WeatherStateArrayCode::HEAVY_SNOW:
			r = g = b = 255_c;
			hatch_r = hatch_g = hatch_b = 40_c;
			do_hatch = true;
			break;
		case slots::WeatherStateArrayCode::DRIZZLE:
			r = g = 30_c;
			b = 90_c;
			break;
		case slots::WeatherStateArrayCode::LIGHT_RAIN:
			r = g = 30_c;
			b = 150_c;
			break;
		case slots::WeatherStateArrayCode::RAIN:
			r = g = 55_c;
			b = 220_c;
			break;
		case slots::WeatherStateArrayCode::HEAVY_RAIN:
			r = g = 60_c;
			b = 255_c;
			hatch_r = hatch_g = 50_c;
			hatch_b = 100_c;
			do_hatch = true;
			break;
		default:
			break;
	}

	if (do_hatch) 
		draw::hatched_rect(matrix.get_inactive_buffer(), start, 40, end, 51, r, g, b, hatch_r, hatch_g, hatch_b);
	else
		draw::rect(matrix.get_inactive_buffer(), start, 40, end, 51, r, g, b);
}

void screen::WeatherScreen::draw_graphaxes() {
	// graph axes on pos 79 x, 24 y
	
	draw::rect(matrix.get_inactive_buffer(), 80, 24, 128, 25, 34_c, 34_c, 34_c);
	draw::rect(matrix.get_inactive_buffer(), 79, 0, 80, 24,   34_c, 34_c, 34_c);

	const float * tempgraph_data = *servicer.slot<float *>(slots::WEATHER_TEMP_GRAPH);

	float min_ = 10000, max_ = -10000;
	for (uint8_t i = 0; i < 24; ++i) {
		min_ = std::min(min_, tempgraph_data[i]);
		max_ = std::max(max_, tempgraph_data[i]);
	}

	// The actual min/max is rounded down/up +/- 1
	min_ = std::floor(min_) - 1.f;
	max_ = std::ceil(max_) + 1.f;
	
	auto tickmark = [](int16_t x, int16_t y, float v){
		matrix.get_inactive_buffer().r(x, y) = 34_c;
		matrix.get_inactive_buffer().g(x, y) = 34_c;
		matrix.get_inactive_buffer().b(x, y) = 34_c;

		char buf[4] = {0};
		snprintf(buf, 4, "%.0f", v);

		if (x == 78) {
			int16_t sz = draw::text_size(buf, font::lcdpixel_6::info);
			x -= sz - 1;
			y += 3;
			if (y < 5) y = 5;
		}
		else {
			y += 6;
		}

		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y, 60_c, 60_c, 60_c);
	};

	tickmark(78, 23, min_);

	int16_t previous = 23;
	float previous_val = min_, previous_height = 23.f;
	float diff = 23.0 / (max_ - min_);
	if (!(diff > 0.05)) return;
	
	// Continue adding tickmarks such that:
	// 	they all have an integer value
	// 	they don't intersect
	// 	they have at least one pixel gap
	
	while (previous_height >= diff) {
		previous_val += 1.f;
		previous_height -= diff;
		int16_t target = std::round(previous_height);
		if ((previous + 3) - std::max(5, target + 3) > 5) {
			previous = target;
			tickmark(78, previous, previous_val);
		}
	}

	struct tm timedat;
	time_t now = rtc_time / 1000;
	gmtime_r(&now, &timedat);
	int first = timedat.tm_hour;

	tickmark(80, 25, first);
	tickmark(92, 25, (first + 6) % 24);
	tickmark(104, 25, (first + 12) % 24);
	tickmark(116, 25, (first + 18) % 24);
}

void screen::WeatherScreen::draw_graph(uint8_t hour) {
	const float * tempgraph_data = *servicer.slot<float *>(slots::WEATHER_TEMP_GRAPH);

	float min_ = 10000, max_ = -10000;
	for (uint8_t i = 0; i < 24; ++i) {
		min_ = std::min(min_, tempgraph_data[i]);
		max_ = std::max(max_, tempgraph_data[i]);
	}

	// The actual min/max is rounded down/up +/- 1
	min_ = std::floor(min_) - 1.f;
	max_ = std::ceil(max_) + 1.f;

	int16_t begin = hour * 2 + 80;
	int16_t end   = begin + 2;

	float diff = 23.0 / (max_ - min_);

	int16_t y0 = std::round((tempgraph_data[hour] - min_) * diff);
	int16_t y1 = (hour == 23 ? y0 : std::round((tempgraph_data[hour + 1] - min_) * diff));
	y0 = 23 - y0;
	y1 = 23 - y1;
	if (hour == 23) y1 = y0;

	float ctemp = tempgraph_data[hour];
	if (ctemp >= 10.0 && ctemp < 16.0) {
		draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, 255_c, 255_c, 255_c);
	}
	else if (ctemp > -10.0 && ctemp < 10.0) {
		draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, 100_c, 100_c, 240_c);
	}
	else if (ctemp >= 16.0 && ctemp < 30.0) {
		draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, 50_c, 240_c, 50_c);
	}
	else if (ctemp > 30.0) {
		draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, 250_c, 30_c, 30_c);
	}
	else {
		draw::line(matrix.get_inactive_buffer(), begin, y0, end, y1, 40_c, 40_c, 255_c);
	}
}

void screen::WeatherScreen::draw() {
	// Lock the contents
	srv::ServicerLockGuard g(servicer);

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
			draw_graphaxes();
			for (int i = 0; i < 24; ++i) {
				draw_graph(i);
			}
		}
	}
}
