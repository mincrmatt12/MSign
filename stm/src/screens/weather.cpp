#include "weather.h"
#include "../srv.h"
#include "../tasks/timekeeper.h"
#include "../common/slots.h"
#include "../fonts/latob_15.h"
#include "../fonts/dejavu_10.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/lcdpixel_6.h"
#include "../intmath.h"
#include "../tasks/screen.h"

#include "../draw.h"
#include <time.h>

#include <cmath>
extern uint64_t rtc_time;
extern tasks::Timekeeper timekeeper;
extern matrix_type matrix;
extern srv::Servicer servicer;
extern tasks::DispMan dispman;

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
		slots::WEATHER_MPREC_GRAPH,
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
		slots::WEATHER_MPREC_GRAPH,
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
		slots::WEATHER_MPREC_GRAPH,
		slots::WEATHER_RTEMP_GRAPH,
		slots::WEATHER_WIND_GRAPH,
		slots::WEATHER_HPREC_GRAPH,
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

	snprintf(disp_buf, 16, "\xfe %d \xfd %d", intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->ltemp), intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->htemp));
	text_size = draw::text_size(disp_buf, font::dejavusans_10::info);

	snprintf(disp_buf, 16, "%d ", intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->ltemp));
	auto nxt = draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, 42 - text_size / 2, 30, "\xfe ", 0x7f7ff0_cc, disp_buf, 127_c);

	snprintf(disp_buf, 16, "%d", intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->htemp));
	draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, nxt, 30, "\xfd ", led::color_t{255_c, 127_c, 10_c}, disp_buf, led::color_t{127_c, 127_c, 127_c});

	{
		auto v = intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->crtemp, (int16_t)10);
		snprintf(disp_buf, 16, "%d.%d", v / 10, std::abs(v % 10));
		text_size = draw::text_size(disp_buf, font::lcdpixel_6::info);
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lcdpixel_6::info, 44 - text_size / 2, 20, 127_c);
	}

	int16_t y = 3 + draw::fastsin(timekeeper.current_time, 1900, 3);

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
			auto current_speed_div = std::min<uint64_t>(10, std::max<uint64_t>(5, 40 - (strlen((char*)*servicer[slots::WEATHER_STATUS]) / 7)));
			if (dispman.interacting()) current_speed_div = 9;
			int16_t t_pos = draw::scroll(timekeeper.current_time / current_speed_div, text_size);
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

void screen::WeatherScreen::fill_hourlybar(int16_t x0, int16_t y0, int16_t x1, int16_t y1, slots::WeatherStateArrayCode code, const char* &text_out, int64_t hourstart, bool vert) {
	led::color_t col(127), hatch(127);
	bool do_hatch = false;

	const auto &times = *servicer.slot<slots::WeatherTimes>(slots::WEATHER_TIME_SUN);

	switch (code) {
		case slots::WeatherStateArrayCode::CLEAR:
			text_out = "Clear";
			{
				// Do sunrise filling
				if (vert) {
					for (auto y = y0; y < y1; ++y) {
						int64_t effective = (((int64_t)(y - y0) * (1000*60*60)) / (y1 - y0) + hourstart);
						if (effective > times.sunrise && effective < times.sunset) {
							draw::rect(matrix.get_inactive_buffer(), x0, y, x1, y + 1, {210_c, 200_c, 0});
						}
						else {
							draw::rect(matrix.get_inactive_buffer(), x0, y, x1, y + 1, 0);
						}
					}
				}
				else {
					for (auto x = x0; x < x1; ++x) {
						int64_t effective = (((int64_t)(x - x0) * (1000*60*60)) / (x1 - x0) + hourstart);
						if (effective > times.sunrise && effective < times.sunset) {
							draw::rect(matrix.get_inactive_buffer(), x, y0, x + 1, y1, {210_c, 200_c, 0});
						}
						else {
							draw::rect(matrix.get_inactive_buffer(), x, y0, x + 1, y1, 0);
						}
					}
				}
			}
			return;
		case slots::WeatherStateArrayCode::PARTLY_CLOUDY:
			text_out = "Partly cloudy";
			col = led::color_t(130); break;
		case slots::WeatherStateArrayCode::MOSTLY_CLOUDY:
			text_out = "Mostly cloudy";
			col = led::color_t(80); break;
		case slots::WeatherStateArrayCode::OVERCAST:
			text_out = "Overcast";
			col = led::color_t(50); break;

		case slots::WeatherStateArrayCode::SNOW:
			text_out = "Snow";
			col = led::color_t(200);
			hatch = led::color_t(80);
			do_hatch = true;
		    break;
		case slots::WeatherStateArrayCode::HEAVY_SNOW:
			text_out = "Heavy snow";
			col = led::color_t(255);
			hatch = led::color_t(40);
			do_hatch = true;
			break;
		case slots::WeatherStateArrayCode::DRIZZLE:
			text_out = "Drizzle";
			col.r = col.g = 30_c;
			col.b = 90_c;
			break;
		case slots::WeatherStateArrayCode::LIGHT_RAIN:
			text_out = "Light rain";
			col.r = col.g = 30_c;
			col.b = 150_c;
			break;
		case slots::WeatherStateArrayCode::RAIN:
			text_out = "Rain";
			col.r = col.g = 55_c;
			col.b = 220_c;
			break;
		case slots::WeatherStateArrayCode::HEAVY_RAIN:
			text_out = "Heavy rain";
			col.r = col.g = 60_c;
			col.b = 255_c;
			hatch.r = hatch.g = 50_c;
			hatch.b = 100_c;
			do_hatch = true;
			break;
		default:
			text_out = "Unknown";
			break;
	}

	if (do_hatch) 
		draw::hatched_rect(matrix.get_inactive_buffer(), x0, y0, x1, y1, col, hatch);
	else
		draw::rect(matrix.get_inactive_buffer(), x0, y0, x1, y1, col);
}

void screen::WeatherScreen::draw_hourlybar(uint8_t hour) {
	int start = 4 + hour * 5;
	int end =   9 + hour * 5;

    slots::WeatherStateArrayCode code = (*servicer.slot<slots::WeatherStateArrayCode *>(slots::WEATHER_ARRAY))[hour]; 
	
	struct tm timedat;
	time_t now = rtc_time / 1000;
	gmtime_r(&now, &timedat);
	hour = (timedat.tm_hour + hour) % 24;
	int64_t hourstart = (int64_t)hour * (1000*60*60);

	const char* dummy;
	fill_hourlybar(start, 40, end, 51, code, dummy, hourstart);
}

void screen::WeatherScreen::draw_big_hourlybar() {
	// BIG HBAR:
	//
	// ---------------------------------
	// |  
	// |  |---| --- TEXT TEXT
	// |  |///|
	// |  |///|
	// |   ... scrolls
	
	const int leftside = 20; // configurable to allow for potential icons on left gutter (like sunrise/sunset)
	const int topside = 5;
	const int barwidth = 8;
	const int segmentheight = 10;
	const int linewidth = 3;
	const int labelheight = 9;
	const int labelbase = 2;

	slots::WeatherStateArrayCode last = slots::WeatherStateArrayCode::UNK;
	auto& blk = servicer.slot<slots::WeatherStateArrayCode *>(slots::WEATHER_ARRAY);
	const auto &times = *servicer.slot<slots::WeatherTimes>(slots::WEATHER_TIME_SUN);

	struct tm timedat;
	time_t now = rtc_time / 1000;
	gmtime_r(&now, &timedat);

	bool forcehourlabel = true;

	for (int hour = 0; hour < 24; ++hour) {
		int offset_y = topside + hour * segmentheight - expanded_hrbar_scroll / 128;
		if (offset_y + segmentheight < 0) continue; // don't update last to keep a label onscreen
		if (offset_y > 63) break;

		slots::WeatherStateArrayCode code = blk[hour], next = hour == 23 ? code : blk[hour + 1];
		int effhour = (timedat.tm_hour + hour) % 24;
		int64_t hourstart = (int64_t)effhour * (1000*60*60);
		const char *label;

		// Draw bar + lines
		draw::rect(matrix.get_inactive_buffer(), leftside, std::max(0, offset_y), leftside + 1, std::max(0, offset_y + segmentheight), 0xff_c);
		draw::rect(matrix.get_inactive_buffer(), leftside + 1 + barwidth, std::max(0, offset_y), leftside + 1 + barwidth + 1, std::max(0, offset_y + segmentheight), 0xff_c);
		fill_hourlybar(leftside + 1, std::max(0, offset_y), leftside + 1 + barwidth, std::max(0, offset_y + segmentheight), code, label, hourstart, true);

		// Check if we need a label
		bool needlabel = std::exchange(last, code) != code;
		if (needlabel) {
			int labely = offset_y;
			if (offset_y < 1) {
				labely = 1;
				int maxhgt = (next == code ? 2 * segmentheight : segmentheight);
				if (labely + labelheight > offset_y + maxhgt) {
					labely = offset_y + maxhgt - labelheight;
				}
			}

			draw::rect(matrix.get_inactive_buffer(), leftside + barwidth + 2 + 2, labely, leftside + barwidth + 2 + 2 + linewidth, labely + 1, 0x99_c);
			draw::text(matrix.get_inactive_buffer(), label, font::tahoma_9::info, leftside + barwidth + 2 + 2 + linewidth + 2, labely + labelheight - labelbase, 
				(hourstart < times.sunrise || hourstart > times.sunset) ? 0x55_c : 0xff_c // darker at night
			);
		}

		// Now do the same but for left-side labels (hour labels)
		if (hour % 4 == 0 || forcehourlabel) {
			int labely = offset_y;
			int firstpos = std::max(1, topside + 1 - expanded_hrbar_scroll / 128);
			if (forcehourlabel) {
				labely = firstpos;
				forcehourlabel = false;
			}
			else {
				if (labely < firstpos + 6) continue;
			}

			char buf[10];
			snprintf(buf, 10, "%02d:00", effhour);
			int x = leftside - draw::text_size(buf, font::lcdpixel_6::info);
			draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, labely + 5, 0xff_c);
		}
	}
}

void screen::WeatherScreen::draw_graph_yaxis(int16_t x, int16_t y0, int16_t y1, int32_t &ymin, int32_t &ymax, bool decimals) {
	// Assume ymax/ymin have been set properly; we will do the ceil/floor operations
	
	// Draw axis at coordinate x.
	draw::rect(matrix.get_inactive_buffer(), x, y0, x+1, y1, 50_c);

	// Update min/max
	if (!decimals) {
		ymin = intmath::floor10(ymin)*100 - 100;
		ymax = intmath::ceil10(ymax)*100 + 100;
	}
	else {
		ymin = 0;
		ymax = intmath::ceil10<int32_t>(ymax, 10)*10 + 10;
	}

	// Draw tickmarks
	int32_t space = (y1 - y0);
	int32_t tickmark_min = intmath::ceil10<int32_t>(6 * (ymax - ymin) / space, decimals ? 10 : 100);

	for (int i = 0;; ++i) {
		int32_t value = (tickmark_min * i) * (decimals ? 10 : 100);
		int32_t pos = y0 + intmath::round10((space*100 - 100) - ((space * value * 100) / (ymax - ymin)));

		if (pos < y0) break;

		matrix.get_inactive_buffer().at(x - 1, pos) = led::color_t(95_c);

		char buf[10] = {0};
		if (decimals) {
			auto v = intmath::round10<int32_t>(ymin + value, 10);
			snprintf(buf, 10, "%d.%d", v / 10, std::abs(v % 10));
		}
		else {
			snprintf(buf, 10, "%d", intmath::round10(ymin + value));
		}

		int16_t sz = draw::text_size(buf, font::lcdpixel_6::info);
		int16_t xeff = x - sz - 1;
		pos += 3;
		if (pos < 5) pos = 5;

		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, xeff, pos, 120_c);
	}
}

void screen::WeatherScreen::draw_graph_xaxis(int16_t y, int16_t x0, int16_t x1, int start, bool ashours) {
	draw::rect(matrix.get_inactive_buffer(), x0, y, x1, y + 1, 50_c);

	// min width for full hour text is 108 pixels
	
	if ((x1 - x0) > 56) {
		for (int i = 0; i < 6; ++i) {
			int x = x0 + 1 + (i*(x1 - x0 - 1)) / 6;
			int val = ashours ? (start + i * 4) % 24 : (start + i * 10) % 60;

			matrix.get_inactive_buffer().at(x, y + 1) = led::color_t(95_c);

			char buf[10] = {0};
			snprintf(buf, 10, "%02d", val);

			if (ashours && (x1 - x0) > 108) {
				draw::multi_text(matrix.get_inactive_buffer(), font::lcdpixel_6::info, x, y + 7, buf, 0x9dd3e3_cc, ":00", 120_c);
			}
			else if (!ashours && (x1 - x0) > 96) {
				draw::multi_text(matrix.get_inactive_buffer(), font::lcdpixel_6::info, x, y + 7, "+", 120_c, buf, 0x9dd3e3_cc, "m", 120_c);
			}
			else {
				draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y + 7, 120_c);
			}
		}
	}
	else {
		for (int i = 0; i < 6; ++i) {
			int x = x0 + 1 + (i*(x1 - x0 - 1)) / 4;
			int val = ashours ? (start + i * 6) % 24 : (start + i * 15) % 60;
			 
			matrix.get_inactive_buffer().at(x, y + 1) = led::color_t(95_c);

			char buf[10] = {0};
			snprintf(buf, 10, "%02d", val);

			draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y + 7, 120_c);
		}
	}
}

void screen::WeatherScreen::draw_graph_lines(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const int16_t * data, size_t amount, int32_t ymin, int32_t ymax, bool show_temp_colors) {
	int32_t space = (y1 - y0);

	// Draw lines (assume x0/y0 are _inclusive_, i.e. not the axes)
	
	for (int idx = 0; idx < amount; ++idx) {
		int begin = idx * (x1 - x0) / amount;
		int end = (idx + 1) * (x1 - x0) / amount;
		begin += x0;
		end += x0;

		int16_t iy0 = intmath::round10((int32_t(data[idx] - ymin) * space * 100) / (ymax - ymin));
		int16_t iy1 = (idx == 23 ? iy0 : intmath::round10((int32_t(data[idx + 1] - ymin) * space * 100) / (ymax - ymin)));

		iy0 = y1 - 1 - iy0;
		iy1 = y1 - 1 - iy1;
		if (idx == 23) iy1 = iy0;

		auto cval = data[idx];
		if (show_temp_colors) {
			if (cval >= 1000 && cval < 1600) {
				draw::line(matrix.get_inactive_buffer(), begin, iy0, end, iy1, 255_c);
			}
			else if (cval > -1000 && cval < 1000) {
				draw::line(matrix.get_inactive_buffer(), begin, iy0, end, iy1, {100_c, 100_c, 240_c});
			}
			else if (cval >= 1600 && cval < 3000) {
				draw::line(matrix.get_inactive_buffer(), begin, iy0, end, iy1, {50_c, 240_c, 50_c});
			}
			else if (cval >= 3000) {
				draw::line(matrix.get_inactive_buffer(), begin, iy0, end, iy1, {250_c, 30_c, 30_c});
			}
			else {
				draw::line(matrix.get_inactive_buffer(), begin, iy0, end, iy1, {40_c, 40_c, 255_c});
			}
		}
		else {
			draw::line(matrix.get_inactive_buffer(), begin, iy0, end, iy1, 255_c);
		}
	}
}

namespace screen {
	int wigglerange_from(int stddev, int amount, int probability) {
		// We can express unconfidence as stddev / amount, and then rescale and cap
		
		stddev = (stddev * 255) / probability;

		int unconf = (stddev * 100) / amount;

		if (unconf > 60) {
			unconf = 60 + (unconf - 60) / 2;
		}
		if (unconf > 85) {
			unconf = 85 + (unconf - 85) / 2;
		}

		unconf = std::min(unconf, 120) / 3;

		return (amount * unconf) / 100;
	}
}

void screen::WeatherScreen::draw_graph_precip(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const slots::PrecipData * data, size_t amount, int32_t ymin, int32_t ymax) {
	int32_t space = (y1 - y0);

	for (int x = x0; x < x1; ++x) {
		auto i0 = ((x - x0) * amount) / (x1 - x0 - 1);
		if (i0 >= amount) i0 = amount-1;
		auto i1 = i0 + 1;
		if (i1 >= amount) i1 = i0;

		int px = (i0 * (x1 - x0 - 1)) / amount;
		int nx = (i1 * (x1 - x0 - 1)) / amount;

		auto interp = [&](auto memptr) -> int {
			if (i0 == i1) return data[i0].*memptr;
			return data[i0].*memptr + (((int)(data[i1].*memptr) - (int)(data[i0].*memptr)) * (x - px - x0)) / (nx - px);
		};

		auto prob = interp(&slots::PrecipData::probability);

		uint8_t colprob = prob < 30 ? 30 : (uint8_t)prob;

		int amt = interp(&slots::PrecipData::amount);
		int wigrange = wigglerange_from(interp(&slots::PrecipData::stddev), amt, prob);
		int wiggle = draw::fastsin(timekeeper.current_time + (x-x0)*20, 300, std::max(wigrange, 0));
		amt += wiggle;
		if (amt < 0) continue;
		int16_t pos = y1 - 1 - intmath::round10((int32_t(amt - ymin) * space * 100) / (ymax - ymin));
		if (pos >= y1) continue;
		pos = std::max<int16_t>(0, pos);
		
		if (data[i0].is_snow) {
			draw::hatched_rect(matrix.get_inactive_buffer(), x, pos, x+1, y1, draw::cvt((0_ccu).mix(200_cu, colprob)), draw::cvt((0_ccu).mix(80_cu, colprob)));
		}
		else {
			draw::rect(matrix.get_inactive_buffer(), x, pos, x+1, y1, draw::cvt((0_ccu).mix({55_cu, 55_cu, 220_cu}, colprob))); // todo: diff colors for diff intensities.
		}
	}
}

void screen::WeatherScreen::draw_small_tempgraph() {
	const int16_t * tempgraph_data = *servicer.slot<int16_t *>(slots::WEATHER_TEMP_GRAPH);

	int32_t min_ = INT16_MAX, max_ = INT16_MIN;
	for (uint8_t i = 0; i < 24; ++i) {
		min_ = std::min<int32_t>(min_, tempgraph_data[i]);
		max_ = std::max<int32_t>(max_, tempgraph_data[i]);
	}

	struct tm timedat;
	time_t now = rtc_time / 1000;
	gmtime_r(&now, &timedat);
	int first = timedat.tm_hour;

	draw_graph_xaxis(24, 79, 128, first, true);
	draw_graph_yaxis(79, 0, 24, min_, max_);

	draw_graph_lines(80, 0, 128, 24, tempgraph_data, 24, min_, max_, true);
}

void screen::WeatherScreen::draw_small_precgraph() {
	const auto blk_precip = *servicer.slot<slots::PrecipData *>(slots::WEATHER_MPREC_GRAPH);

	uint8_t max_prob = 0;

	int32_t min_ = 10, max_ = INT16_MIN;
	for (uint8_t i = 0; i < (graph == PRECIP_DAY ? 24 : 30); ++i) {
		max_ = std::max<int32_t>(max_, blk_precip[i].amount + wigglerange_from(blk_precip[i].stddev, blk_precip[i].amount, blk_precip[i].probability) * 4 / 5);
		max_prob = std::max(max_prob, blk_precip[i].probability);
	}
	max_ = std::max<int32_t>(max_, 60); // really little amounts of rain often have high stddev and that looks really stupid

	if (max_prob < 39) {
		draw_small_tempgraph(); // if there's less than a 15% chance of rain, just draw the temperature graph
		return;
	}

	draw_graph_xaxis(24, 79, 128, 0, false);
	draw_graph_yaxis(79, 0, 24, min_, max_, true);

	draw_graph_precip(80, 0, 128, 24, blk_precip, 30, min_, max_);
}

void screen::WeatherScreen::draw_big_graphs() {
	// Ensure loaded
	if (
		(graph == FEELS_TEMP && !servicer.slot(slots::WEATHER_TEMP_GRAPH)) ||
		(graph == REAL_TEMP && !servicer.slot(slots::WEATHER_RTEMP_GRAPH)) ||
		(graph == WIND && !servicer.slot(slots::WEATHER_WIND_GRAPH)) ||
		(graph == PRECIP_DAY && (!servicer.slot(slots::WEATHER_HPREC_GRAPH) || servicer.slot(slots::WEATHER_HPREC_GRAPH).datasize == 0)) ||
		(graph == PRECIP_HOUR && (!servicer.slot(slots::WEATHER_MPREC_GRAPH) || servicer.slot(slots::WEATHER_MPREC_GRAPH).datasize == 0))
	) {
		const char * txt = "loading";

		if (graph == PRECIP_DAY || graph == PRECIP_HOUR) txt = "no data";

		int pos = 64 - draw::text_size(txt, font::dejavusans_10::info) / 2;

		// show loading spinner (TODO)
		draw::text(matrix.get_inactive_buffer(), txt, font::dejavusans_10::info, pos, 34, 0xff_c);
	}
	else {
		// draw graph

		int32_t min_ = INT16_MAX, max_ = INT16_MIN;

		// First, find the correct min/max + hour0/min0
		struct tm timedat;
		time_t now = rtc_time / 1000;
		gmtime_r(&now, &timedat);
		
		const int16_t * blk_16 = nullptr;
		const slots::PrecipData * blk_precip = nullptr;

		switch (graph) {
			case FEELS_TEMP:
				blk_16 = *servicer.slot<int16_t *>(slots::WEATHER_TEMP_GRAPH); break;
			case REAL_TEMP:
				blk_16 = *servicer.slot<int16_t *>(slots::WEATHER_RTEMP_GRAPH); break;
			case WIND:
				blk_16 = *servicer.slot<int16_t *>(slots::WEATHER_WIND_GRAPH); break;
			case PRECIP_HOUR:
				blk_precip = *servicer.slot<slots::PrecipData *>(slots::WEATHER_MPREC_GRAPH); break;
			case PRECIP_DAY:
				blk_precip = *servicer.slot<slots::PrecipData *>(slots::WEATHER_HPREC_GRAPH); break;
				break;
			default:
				return;
		}

		switch (graph) {
			case FEELS_TEMP:
			case REAL_TEMP:
			case WIND:
				for (uint8_t i = 0; i < 24; ++i) {
					min_ = std::min<int32_t>(min_, blk_16[i]);
					max_ = std::max<int32_t>(max_, blk_16[i]);
				}

				break;
			case PRECIP_HOUR:
			case PRECIP_DAY:

				min_ = 10;
				for (uint8_t i = 0; i < (graph == PRECIP_DAY ? 24 : 30); ++i) {
					max_ = std::max<int32_t>(max_, blk_precip[i].amount + wigglerange_from(blk_precip[i].stddev, blk_precip[i].amount, blk_precip[i].probability) * 4 / 5);
				}
				max_ = std::max<int32_t>(max_, 90); // really little amounts of rain often have high stddev and that looks really stupid

				break;
			default:
				return;
		}

		// draw axes

		int leftside = blk_precip == nullptr ? 14 : 17;
		int top = blk_precip == nullptr ? 0 : 5;

		draw_graph_xaxis(64-8, leftside, 128, graph == PRECIP_HOUR ? 0 : timedat.tm_hour, graph != PRECIP_HOUR);
		draw_graph_yaxis(leftside, top, 64-8, min_, max_, blk_precip != nullptr);

		if (blk_16) {
			draw_graph_lines(leftside + 1, 0, 128, 64-8, blk_16, 24, min_, max_, graph != WIND);
		}
		else {
			draw_graph_precip(leftside + 1, top, 128, 64-8, blk_precip, (graph == PRECIP_DAY ? 24 : 30), min_, max_);
			// draw a precipitation bar

			int ps = 32;
			int pl = leftside + 1 + ((128 - leftside + 1) / 2) - (ps / 2);
			int pr = pl + ps;

			draw::gradient_rect(matrix.get_inactive_buffer(), pl, 0, pr, 5, 0, {55_cu, 55_cu, 220_cu});
			draw::rect(matrix.get_inactive_buffer(), pl, 4, pr, 5, 0x77_c); // divider
			draw::text(matrix.get_inactive_buffer(), "0%", font::lcdpixel_6::info, pl - 8, 5, 0xee_c);
			draw::text(matrix.get_inactive_buffer(), "100%", font::lcdpixel_6::info, pr, 5, 0xee_c);
		}
	}

	// Show the current legend
	if (xTaskGetTickCount() - show_graph_selector_timer < pdMS_TO_TICKS(2500)) {
		const char * txt;

		switch (graph) {
			case FEELS_TEMP:
				txt = "feels like (C)";
				break;
			case REAL_TEMP:
				txt = "temp (C)";
				break;
			case WIND:
				txt = "wind (km/h)";
				break;
			case PRECIP_DAY:
				txt = "precip (day, mm/h)";
				break;
			case PRECIP_HOUR:
				txt = "precip (hour, mm/h)";
			default:
				break;
		}

		auto siz = draw::text_size(txt, font::tahoma_9::info) + 6 /* line */ + 4 /* border */;

		draw::rect(matrix.get_inactive_buffer(), 128 - 3 - siz, 64 - 14, 123, 63, 0);
		draw::dashed_outline(matrix.get_inactive_buffer(), 128 - 3 - siz, 64 - 14, 123, 63, 0xff_c);

		draw::line(matrix.get_inactive_buffer(), 128 - 3 - siz + 2, 64 - 7, 128 - 3 - siz + 5, 64 - 7, 0x66ee66_cc);
		draw::text(matrix.get_inactive_buffer(), txt, font::tahoma_9::info, 128-3-siz + 7, 64 - 4, 0xff_c);
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
			if (servicer.slot(slots::WEATHER_TEMP_GRAPH) && servicer.slot(slots::WEATHER_MPREC_GRAPH)) {
				if (rtc_time % 7000 < 3500) {
					draw_small_tempgraph();
				}
				else {
					draw_small_precgraph();
				}
			}
			else if (servicer.slot(slots::WEATHER_TEMP_GRAPH))  {
				draw_small_tempgraph();
			}
			else if (servicer.slot(slots::WEATHER_MPREC_GRAPH)) {
				draw_small_precgraph();
			}
		}
		break;
	case BIG_GRAPH:
		draw_big_graphs();
		break;
	case BIG_HOURLY:
		draw_big_hourlybar();
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

				switch (subscreen) {
					default: 
					{
						expanded_hrbar_scroll = 0;
						break;
					}
					case BIG_GRAPH: 
					{
						graph = FEELS_TEMP;
						show_graph_selector_timer = xTaskGetTickCount();
						servicer.set_temperature_all<
							slots::WEATHER_RTEMP_GRAPH,
							slots::WEATHER_HPREC_GRAPH,
							slots::WEATHER_WIND_GRAPH
						>(bheap::Block::TemperatureWarm);
						break;
					}
				}

			}
			else if (ui::buttons[ui::Buttons::POWER]) return true;
			break;
		case BIG_GRAPH:
			{
				auto fix_temp = [](GraphType g, uint32_t temp){
					switch (g) {
						default: break;
						case REAL_TEMP: servicer.set_temperature(slots::WEATHER_RTEMP_GRAPH, temp); break;
						case WIND: servicer.set_temperature(slots::WEATHER_WIND_GRAPH, temp); break;
						case PRECIP_DAY: servicer.set_temperature(slots::WEATHER_HPREC_GRAPH, temp); break;
					}
				};

				if (ui::buttons[ui::Buttons::POWER]) { 
					servicer.set_temperature_all<
						slots::WEATHER_RTEMP_GRAPH,
						slots::WEATHER_HPREC_GRAPH,
						slots::WEATHER_WIND_GRAPH
					>(bheap::Block::TemperatureCold);
					subscreen = MAIN;
				}
				else if (ui::buttons[ui::Buttons::SEL]) {
					show_graph_selector_timer = xTaskGetTickCount();
				}
				else if (ui::buttons[ui::Buttons::NXT]) {
					show_graph_selector_timer = xTaskGetTickCount();
					fix_temp(graph, bheap::Block::TemperatureWarm);
					graph = (GraphType)(1 + (int)graph);
					if (graph == MAX_GTYPE) graph = FEELS_TEMP;
					fix_temp(graph, bheap::Block::TemperatureHot);
				}
				else if (ui::buttons[ui::Buttons::PRV]) {
					show_graph_selector_timer = xTaskGetTickCount();
					fix_temp(graph, bheap::Block::TemperatureWarm);
					if (graph == FEELS_TEMP) graph = PRECIP_DAY;
					else graph = (GraphType)(-1 + (int)graph);
					fix_temp(graph, bheap::Block::TemperatureHot);
				}

				break;
			}
		case BIG_HOURLY:
			if (ui::buttons[ui::Buttons::NXT]) {
				expanded_hrbar_scroll += 128*8;
			}
			else if (ui::buttons.held(ui::Buttons::NXT, pdMS_TO_TICKS(500))) {
				expanded_hrbar_scroll += ui::buttons.frame_time() * 8;
			}
			else if (ui::buttons[ui::Buttons::PRV]) {
				expanded_hrbar_scroll -= 128*8;
			}
			else if (ui::buttons.held(ui::Buttons::PRV, pdMS_TO_TICKS(500))) {
				expanded_hrbar_scroll -= ui::buttons.frame_time() * 8;
			}
			else if (ui::buttons[ui::Buttons::POWER]) subscreen = MAIN;

			if (expanded_hrbar_scroll < 0) expanded_hrbar_scroll = 0;
			if (expanded_hrbar_scroll > (18*10 + 20) * 128) expanded_hrbar_scroll = (18*10 + 20) * 128;
			break;
		default:
			if (ui::buttons[ui::Buttons::POWER]) subscreen = MAIN;
			break;
	}
	return false;
}

void screen::WeatherScreen::refresh() {
	servicer.refresh_grabber(slots::protocol::GrabberID::WEATHER);
}
