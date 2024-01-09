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
#include <limits.h>
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

// w=20, h=20, stride=3, color=144, 144, 144
const uint8_t thunder_base[] = {
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
	0b00011100,0b00000011,0b10000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000
};

// w=20, h=20, stride=3, color=213, 244, 15
const uint8_t thunder_bolt[] = {
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00000000,0b00000000,
	0b00000000,0b00100000,0b00000000,
	0b00000000,0b01000000,0b00000000,
	0b00000000,0b01000000,0b00000000,
	0b00000000,0b11110000,0b00000000,
	0b00000000,0b00010000,0b00000000,
	0b00000000,0b00100000,0b00000000,
	0b00000000,0b00100000,0b00000000,
	0b00000000,0b00000000,0b00000000
};

}

void screen::WeatherScreen::prepare(bool) {
	servicer.set_temperature_all<
		slots::WEATHER_ARRAY,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_MPREC_GRAPH,
		slots::WEATHER_DAYS
	>(bheap::Block::TemperatureWarm);
}

screen::WeatherScreen::WeatherScreen() {
	servicer.set_temperature_all<
		slots::WEATHER_ARRAY,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_MPREC_GRAPH,
		slots::WEATHER_DAYS
	>(bheap::Block::TemperatureHot);
}

screen::WeatherScreen::~WeatherScreen() {
	servicer.set_temperature_all<
		slots::WEATHER_ARRAY,
		slots::WEATHER_INFO,
		slots::WEATHER_STATUS,
		slots::WEATHER_TEMP_GRAPH,
		slots::WEATHER_MPREC_GRAPH,
		slots::WEATHER_RTEMP_GRAPH,
		slots::WEATHER_WIND_GRAPH,
		slots::WEATHER_GUST_GRAPH,
		slots::WEATHER_HPREC_GRAPH,
		slots::WEATHER_DAYS
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

	if (ctemp >= 1000 && ctemp < 1600) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 240_c);
	}
	else if (ctemp < 1000 && ctemp > -1000) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, {100_c, 100_c, 240_c});
	}
	else if (ctemp >= 1600 && ctemp < 2750) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, {50_c, 240_c, 50_c});
	}
	else if (ctemp >= 2750) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, {250_c, 30_c, 30_c});
	}
	else {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, {40_c, 40_c, 255_c});
	}

	{
		auto v = intmath::round10(servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->crtemp, (int16_t)10);
		snprintf(disp_buf, 16, "%d.%d", v / 10, std::abs(v % 10));
		text_size = draw::text_size(disp_buf, font::lcdpixel_6::info);
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lcdpixel_6::info, 44 - text_size / 2, 20, 127_c);
	}

	int16_t y = 3 + draw::fastsin(timekeeper.current_time, 1900, 3);
	bool is_day = true;
	
	if (auto& weather_days = servicer.slot<slots::WeatherDay *>(slots::WEATHER_DAYS)) {
		uint64_t current = rtc_time % (86400ULL * 1000);
		if (current > weather_days[0].sunrise && current < weather_days[0].sunset) is_day = true;
		else is_day = false;

		snprintf(disp_buf, 16, "\xfe %d \xfd %d", intmath::round10(weather_days[0].low_temperature), intmath::round10(weather_days[0].high_temperature));
		text_size = draw::text_size(disp_buf, font::dejavusans_10::info);

		snprintf(disp_buf, 16, "%d ", intmath::round10(weather_days[0].low_temperature));
		auto nxt = draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, 42 - text_size / 2, 30, "\xfe ", 0x7f7ff0_cc, disp_buf, 127_c);

		snprintf(disp_buf, 16, "%d", intmath::round10(weather_days[0].high_temperature));
		draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, nxt, 30, "\xfd ", led::color_t{255_c, 127_c, 10_c}, disp_buf, led::color_t{127_c, 127_c, 127_c});
	}

	switch (servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->icon) {
		case slots::WeatherStateCode::UNK:
			break;
		case slots::WeatherStateCode::CLEAR:
			if (is_day) draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::clear_day, 20, 20, 3, 1, y, {220_c, 250_c, 0_c});
			else draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::night, 20, 20, 3, 1, y, {79_c, 78_c, 79_c});
			break;
		case slots::WeatherStateCode::PARTLY_CLOUDY:
		case slots::WeatherStateCode::MOSTLY_CLOUDY:
		case slots::WeatherStateCode::CLOUDY:
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::cloudy, 20, 20, 3, 1, y, is_day ? 245_c : 79_c);
			break;
		case slots::WeatherStateCode::DRIZZLE:
		case slots::WeatherStateCode::LIGHT_RAIN:
		case slots::WeatherStateCode::RAIN:
		case slots::WeatherStateCode::HEAVY_RAIN:
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::rain, 20, 20, 3, 1, y, {43_c, 182_c, 255_c});
			break;
		case slots::WeatherStateCode::SNOW:
		case slots::WeatherStateCode::FLURRIES:
		case slots::WeatherStateCode::LIGHT_SNOW:
		case slots::WeatherStateCode::HEAVY_SNOW:
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::snow, 20, 20, 3, 1, y, 255_c);
			break;
		case slots::WeatherStateCode::FREEZING_DRIZZLE:
		case slots::WeatherStateCode::FREEZING_LIGHT_RAIN:
		case slots::WeatherStateCode::FREEZING_RAIN:
		case slots::WeatherStateCode::FREEZING_HEAVY_RAIN:
			break; // todo freezing rain
		case slots::WeatherStateCode::LIGHT_FOG:
		case slots::WeatherStateCode::FOG:
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::fog, 20, 20, 3, 1, y, {118_c, 118_c, 118_c});
			break;
		case slots::WeatherStateCode::LIGHT_ICE_PELLETS:
		case slots::WeatherStateCode::ICE_PELLETS:
		case slots::WeatherStateCode::HEAVY_ICE_PELLETS:
			// todo ice pellets
			break;
		case slots::WeatherStateCode::THUNDERSTORM:
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::thunder_base, 20, 20, 3, 1, y, 144_c);
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::thunder_bolt, 20, 20, 3, 1, y, {213_c, 244_c, 15_c});
			break;
		case slots::WeatherStateCode::WINDY:
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::weather::wind, 20, 20, 3, 1, y, {118_c, 118_c, 118_c});
			break;
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
	time_t now = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->updated_at / 1000;
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

void screen::WeatherScreen::fill_hourlybar(int16_t x0, int16_t y0, int16_t x1, int16_t y1, slots::WeatherStateCode code, const char* &text_out, int64_t hourstart, bool vert) {
	led::color_t col(127), hatch(127);
	bool do_hatch = false;

	const auto &times = servicer.slot<slots::WeatherDay *>(slots::WEATHER_DAYS)[0];
	using enum slots::WeatherStateCode;

	switch (code) {
		case CLEAR:
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
		case PARTLY_CLOUDY:
			text_out = "Partly cloudy";
			col = led::color_t(100_c); break;
		case MOSTLY_CLOUDY:
			text_out = "Mostly cloudy";
			col = led::color_t(30_c); break;
		case CLOUDY:
			text_out = "Cloudy";
			col = led::color_t(70_c); break;
		case FOG:
			do_hatch = true;
			hatch = 245_c;
		case LIGHT_FOG:
			text_out = code == FOG ? "Fog" : "Light fog";
			col = led::color_t(215_c);
		    break;
		case FLURRIES:
		case LIGHT_SNOW:
			text_out = code == LIGHT_SNOW ? "Light snow" : "Flurries";
			col = led::color_t(230_c);
			hatch = led::color_t(90_c);
			do_hatch = true;
		    break;
		case SNOW:
			text_out = "Snow";
			col = led::color_t(200_c);
			hatch = led::color_t(80_c);
			do_hatch = true;
		    break;
		case HEAVY_SNOW:
			text_out = "Heavy snow";
			col = led::color_t(255_c);
			hatch = led::color_t(40_c);
			do_hatch = true;
			break;
		case DRIZZLE:
			text_out = "Drizzle";
			col.r = col.g = 30_c;
			col.b = 90_c;
			break;
		case LIGHT_RAIN:
			text_out = "Light rain";
			col.r = col.g = 30_c;
			col.b = 150_c;
			break;
		case RAIN:
			text_out = "Rain";
			col.r = col.g = 55_c;
			col.b = 220_c;
			break;
		case HEAVY_RAIN:
			text_out = "Heavy rain";
			col.r = col.g = 60_c;
			col.b = 255_c;
			hatch.r = hatch.g = 50_c;
			hatch.b = 100_c;
			do_hatch = true;
			break;
		case FREEZING_DRIZZLE:
			text_out = "Freezing drizzle";
			col.r = col.g = 30_c;
			col.b = 90_c;
			hatch = 0xaaaaf2_cc;
			do_hatch = true;
			break;
		case FREEZING_LIGHT_RAIN:
		case LIGHT_ICE_PELLETS:
			text_out = code == FREEZING_LIGHT_RAIN ? "Freezing light rain" : "Light ice pellets";
			col.r = col.g = 30_c;
			col.b = 150_c;
			hatch = 0xaaaaf2_cc;
			do_hatch = true;
			break;
		case FREEZING_RAIN:
		case ICE_PELLETS:
			text_out = code == FREEZING_RAIN ? "Freezing rain" : "Ice pellets";
			col.r = col.g = 55_c;
			col.b = 220_c;
			hatch = 0xaaaaf2_cc;
			do_hatch = true;
			break;
		case FREEZING_HEAVY_RAIN:
		case HEAVY_ICE_PELLETS:
			text_out = code == FREEZING_HEAVY_RAIN ? "Freezing heavy rain" : "Heavy ice pellets";
			col.r = col.g = 60_c;
			col.b = 255_c;
			hatch = 0xaaaaf2_cc;
			do_hatch = true;
			break;
		case THUNDERSTORM:
			text_out = "Thunderstorms";
			col = led::color_t(50_c);
			hatch = 0xc8fa14_cc;
			do_hatch = true;
			break;
		case WINDY:
			text_out = "Windy";
			col = led::color_t(100_c);
			hatch = 0;
			do_hatch = true;
			break;
		case UNK:
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

    slots::WeatherStateCode code = (*servicer.slot<slots::WeatherStateCode *>(slots::WEATHER_ARRAY))[hour]; 
	
	struct tm timedat;
	time_t now = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->updated_at / 1000;
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
	
	const static int leftside = 20; 
	const static int topside = 5;
	const static int barwidth = 8;
	const static int segmentheight = 10;
	const static int linewidth = 3;
	const static int labelheight = 9;
	const static int labelbase = 2;

	slots::WeatherStateCode last = slots::WeatherStateCode::UNK;
	auto& blk = servicer.slot<slots::WeatherStateCode *>(slots::WEATHER_ARRAY);
	const auto &times = servicer.slot<slots::WeatherDay *>(slots::WEATHER_DAYS)[0];

	struct tm timedat;
	time_t now = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->updated_at / 1000;
	gmtime_r(&now, &timedat);

	bool forcehourlabel = true;

	// Compute the place for sunrise/sunset marks on the bar
	bool sunlinestate = false;

	if (auto eff=timedat.tm_hour*(1000*60*60); eff > times.sunrise && eff < times.sunset) {
		sunlinestate = true;
	}

	for (int hour = 0; hour < 24; ++hour) {
		int offset_y = topside + hour * segmentheight - expanded_hrbar_scroll / 128;
		if (offset_y > 63) break;

		slots::WeatherStateCode code = blk[hour], next = hour == 23 ? code : blk[hour + 1];
		int effhour = (timedat.tm_hour + hour) % 24;
		int64_t hourstart = (int64_t)effhour * (1000*60*60);
		int64_t hourend = (int64_t)(effhour + 1) * (1000*60*60);
		const char *label;

		int tloffset = -1;

		// Check if either time is in the current hour
		if (times.sunrise > hourstart && times.sunrise < hourend) tloffset = (times.sunrise - hourstart) / ((1000*60*60L) / segmentheight);
		else if (times.sunset > hourstart && times.sunset < hourend) tloffset = (times.sunset - hourstart) / ((1000*60*60L) / segmentheight);

		if (offset_y + segmentheight < 0) {
			if (tloffset != -1) sunlinestate = !sunlinestate;
			continue; // don't update last to keep a label onscreen
		}

		// Draw bar + lines
		if (tloffset == -1) {
			draw::rect(matrix.get_inactive_buffer(), leftside, std::max(0, offset_y), leftside + 1, std::max(0, offset_y + segmentheight), sunlinestate ? 0xff_c : 0x55_c);
			draw::rect(matrix.get_inactive_buffer(), leftside + 1 + barwidth, std::max(0, offset_y), leftside + 1 + barwidth + 1, std::max(0, offset_y + segmentheight), sunlinestate ? 0xff_c : 0x55_c);
		}
		else {
			draw::rect(matrix.get_inactive_buffer(), leftside, std::max(0, offset_y), leftside + 1, std::max(0, offset_y + tloffset), sunlinestate ? 0xff_c : 0x55_c);
			draw::rect(matrix.get_inactive_buffer(), leftside + 1 + barwidth, std::max(0, offset_y), leftside + 1 + barwidth + 1, std::max(0, offset_y + tloffset), sunlinestate ? 0xff_c : 0x55_c);
			sunlinestate = !sunlinestate;
			draw::rect(matrix.get_inactive_buffer(), leftside, std::max(0, offset_y + tloffset), leftside + 1, std::max(0, offset_y + segmentheight), sunlinestate ? 0xff_c : 0x55_c);
			draw::rect(matrix.get_inactive_buffer(), leftside + 1 + barwidth, std::max(0, offset_y + tloffset), leftside + 1 + barwidth + 1, std::max(0, offset_y + segmentheight), sunlinestate ? 0xff_c : 0x55_c);
		}
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
	
	for (int i = 0; i < 6; ++i) {
		int x = x0 + 1 + (i*(x1 - x0 - 1)) / 4;
		int val = ashours ? (start + i * 6) % 24 : (start + i * 15) % 60;

		if (val < 0)
			continue;
		 
		matrix.get_inactive_buffer().at(x, y + 1) = led::color_t(95_c);

		char buf[10] = {0};
		snprintf(buf, 10, "%02d", val);

		draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y + 7, 120_c);
	}
}

void screen::WeatherScreen::draw_graph_xaxis_full(int16_t y, int16_t x0, int16_t x1, int x_scroll, int max_scroll, bool ashours) {
	// Draw the line
	draw::rect(matrix.get_inactive_buffer(), x0, y, x1, y + 1, 50_c);

	struct tm timedat;
	auto updated_at = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->updated_at;
	if (ashours) {
		time_t now = updated_at / 1000;
		gmtime_r(&now, &timedat);
	}

	int lsi = ashours ? timedat.tm_hour : intmath::round10<int>(updated_at - rtc_time, 60*1000);
	int lsi_err = 0, lsi_offs;

	if (ashours) {
		// range from x1 - x0 is 24 hours, each lsi is 1 hour, lsi_err is 1/256 of lsi, we want
		lsi_offs = 24 * 256 / (x1 - x0);
	}
	else {
		lsi_offs = 60 * 256 / (x1 - x0);
	}

	lsi_err = x_scroll * lsi_offs;

	auto accumulate = [&]{
		lsi_err += lsi_offs;
		while (lsi_err >= 256) {
			++lsi;
			if (ashours && lsi % 24 == 0) {
				++timedat.tm_wday;
				timedat.tm_wday %= 7;
			}
			lsi_err -= 256;
		}
	};

	accumulate();
	char buf[32];

	int last_label = INT_MIN;

	for (int x = x0; x < x1; ++x, accumulate()) {
		if (lsi % (ashours ? 6 : 12) == 0 && lsi != last_label) {
			if (ashours) {
				snprintf(buf, 32, "%02d:00", lsi % 24);
				if (x - x0 + x_scroll + draw::text_size(buf, font::lcdpixel_6::info) > max_scroll)
					return;
				matrix.get_inactive_buffer().at(x, y + 1) = led::color_t(95_c);
				draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y + 7, 120_c);
				if (lsi % 24 == 0) {
					strftime(buf, 32, "%a", &timedat);
					draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y + 13, 120_c);
				}
			}
			else {
				snprintf(buf, 32, "+%02dm", lsi % 60);
				if (x - x0 + x_scroll + draw::text_size(buf, font::lcdpixel_6::info) > max_scroll)
					return;
				matrix.get_inactive_buffer().at(x, y + 1) = led::color_t(95_c);
				draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y + 7, 120_c);
				if (lsi && lsi % 60 == 0) {
					snprintf(buf, 32, "+%02dh", lsi / 60);
					draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, x, y + 13, 120_c);
				}
			}

			last_label = lsi;
		}
	}
}

void screen::WeatherScreen::draw_graph_lines(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const int16_t * data, int per_screen, int total_amount, int32_t ymin, int32_t ymax, bool show_temp_colors, int x_scroll) {
	int space = (y1 - y0);
	int xspace = (x1 - x0);

	int16_t previous_pos = INT16_MIN;

	// Draw lines (assume x0/y0 are _inclusive_, i.e. not the axes)
	
	for (int x = x0, effx = x_scroll; x < x1; ++x, ++effx) {
		int curindex =    (effx * per_screen) / xspace;
		int curoffset = (effx * per_screen) % xspace;

		if (curindex < 0)
			continue;
		if (curindex >= total_amount)
			continue;
		if (curindex == total_amount - 1)
			curoffset = 0;

		int16_t cval = data[curindex];
		if (curoffset > 0) {
			cval += int(data[curindex + 1] - data[curindex]) * curoffset / xspace;
		}

		led::color_t clr = 255_c;

		if (show_temp_colors) {
			if (cval >= 1000 && cval < 1600) {
				clr = 255_c;
			}
			else if (cval > -1000 && cval < 1000) {
				clr = {100_c, 100_c, 240_c};
			}
			else if (cval >= 1600 && cval < 2750) {
				clr = {50_c, 240_c, 50_c};
			}
			else if (cval >= 2750) {
				clr = {250_c, 30_c, 30_c};
			}
			else {
				clr = {40_c, 40_c, 255_c};
			}
		}
							
		int16_t now_y_pos = y1 - 1 - intmath::round10((int32_t(cval - ymin) * space * 100) / (ymax - ymin));

		if (previous_pos == INT16_MIN || previous_pos == now_y_pos || previous_pos == now_y_pos + 1 || previous_pos + 1 == now_y_pos) {
			matrix.get_inactive_buffer().at(x, now_y_pos) = clr;
		}
		else {
			int16_t middle = (previous_pos + now_y_pos) / 2;
			if (previous_pos < now_y_pos) {
				draw::rect(matrix.get_inactive_buffer(), x - 1, previous_pos, x, middle, clr);
				draw::rect(matrix.get_inactive_buffer(), x, middle, x + 1, now_y_pos + 1, clr);
			}
			else {
				draw::rect(matrix.get_inactive_buffer(), x - 1, middle, x, previous_pos, clr);
				draw::rect(matrix.get_inactive_buffer(), x, now_y_pos, x + 1, middle + 1, clr);
			}
		}

		previous_pos = now_y_pos;
	}
}

void screen::WeatherScreen::draw_graph_precip(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const bheap::TypedBlock<slots::PrecipData *>& precip_data, int per_screen, int desired_amount, int index_offset, int32_t ymin, int32_t ymax, int x_scroll) {
	int32_t space = (y1 - y0);
	int i0, i1, nx, px, x, effx;

	auto access_at = [&](int index) -> const slots::PrecipData& {
		const static slots::PrecipData null{};
		if (index < index_offset) return null;
		if (index - index_offset >= precip_data.datasize / sizeof(slots::PrecipData)) return null;
		return precip_data[index - index_offset];
	};

	auto interp = [&](auto memptr) -> int {
		if (i0 == i1) return access_at(i0).*memptr;
		return access_at(i0).*memptr + (((int)(access_at(i1).*memptr) - (int)(access_at(i0).*memptr)) * (x - px - x0)) / (nx - px);
	};

	for (x = x0, effx = x_scroll; x < x1; ++x, ++effx) {
		i0 = (effx * per_screen) / (x1 - x0 - 1);
		if (i0 >= desired_amount) i0 = desired_amount - 1;
		i1 = i0 + 1;
		if (i1 >= desired_amount) i1 = i0;

		px = (i0 * (x1 - x0 - 1)) / per_screen - x_scroll;
		nx = (i1 * (x1 - x0 - 1)) / per_screen - x_scroll;

		auto prob = interp(&slots::PrecipData::probability);

		uint8_t colprob = prob < 30 ? 30 : (uint8_t)prob;

		int amt = interp(&slots::PrecipData::amount);
		if (amt < 0) continue;
		int16_t pos = y1 - 1 - intmath::round10((int32_t(amt - ymin) * space * 100) / (ymax - ymin));
		if (pos >= y1) continue;
		pos = std::max<int16_t>(0, pos);

		using enum slots::PrecipData::PrecipType;
		slots::PrecipData::PrecipType draw_as = access_at(i0).kind;
		
		// If one of the types of precip is NONE, use the other, otherwise pick whichever is closer.
		if (access_at(i0).kind == NONE && access_at(i1).kind == NONE) {continue;}
		else if (access_at(i0).kind == NONE) {draw_as = access_at(i1).kind;}
		else if (access_at(i1).kind != NONE && x - x0 - px > nx - x + x0) {draw_as = access_at(i1).kind;}
		
		switch (draw_as) {
			case NONE:
				break;
			case FREEZING_RAIN:
				draw::hatched_rect(matrix.get_inactive_buffer(), x, pos, x+1, y1, draw::cvt((0_ccu).mix({55_cu, 55_cu, 220_cu}, colprob)), draw::cvt((0_ccu).mix(0xaaaaf2_ccu, colprob)));
				break;
			case RAIN:
				draw::rect(matrix.get_inactive_buffer(), x, pos, x+1, y1, draw::cvt((0_ccu).mix({55_cu, 55_cu, 220_cu}, colprob)));
				break;
			case SNOW:
				draw::hatched_rect(matrix.get_inactive_buffer(), x, pos, x+1, y1, draw::cvt((0_ccu).mix(200_cu, colprob)), draw::cvt((0_ccu).mix(80_cu, colprob)));
				break;
			case SLEET:
				draw::hatched_rect(matrix.get_inactive_buffer(), x, pos, x+1, y1, draw::cvt((0_ccu).mix({55_cu, 55_cu, 220_cu}, colprob)), draw::cvt((0_ccu).mix(80_cu, colprob)));
				break;
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
	time_t now = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO)->updated_at / 1000;
	gmtime_r(&now, &timedat);
	int first = timedat.tm_hour;

	draw_graph_xaxis(24, 79, 128, first, true);
	draw_graph_yaxis(79, 0, 24, min_, max_);

	draw_graph_lines(80, 0, 128, 24, tempgraph_data, 24, 120, min_, max_, true);
}

void screen::WeatherScreen::draw_small_precgraph() {
	const auto& blk_precip = servicer.slot<slots::PrecipData *>(slots::WEATHER_MPREC_GRAPH);

	uint8_t max_prob = 0;

	const auto& info = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO);
	auto ind_offset = info->minute_precip_offset;

	if (ind_offset >= 12) {
		draw_small_tempgraph(); // if there's no rain within 60 minutes, just show the temperature graph
		return;
	}

	int32_t min_ = 10, max_ = INT16_MIN;
	for (const auto& precip : blk_precip) {
		max_ = std::max<int32_t>(max_, precip.amount);
		max_prob = std::max(max_prob, precip.probability);
	}
	max_ = std::max<int32_t>(max_, 60); // really little amounts of rain would look silly as just 0-0.001 so limit it a bit.

	if (max_prob < 39) {
		draw_small_tempgraph(); // if there's less than a 15% chance of rain, just draw the temperature graph
		return;
	}

	int minutes_behind = intmath::round10<int>(info->updated_at - rtc_time, 60*1000);

	draw_graph_xaxis(24, 79, 128, 0, false);
	draw_graph_yaxis(79, 0, 24, min_, max_, true);

	int x_scroll = (-minutes_behind * (128 - 80)) / 60;

	draw_graph_precip(80, 0, 128, 24, blk_precip, 12, 12, ind_offset, min_, max_, x_scroll);
}

void screen::WeatherScreen::draw_big_graphs() {
	// Ensure loaded
	if (
		(graph == FEELS_TEMP && !servicer.slot(slots::WEATHER_TEMP_GRAPH)) ||
		(graph == REAL_TEMP && !servicer.slot(slots::WEATHER_RTEMP_GRAPH)) ||
		(graph == WIND && !servicer.slot(slots::WEATHER_WIND_GRAPH)) ||
		(graph == GUST && !servicer.slot(slots::WEATHER_GUST_GRAPH)) ||
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
		const auto& info = servicer.slot<slots::WeatherInfo>(slots::WEATHER_INFO);
		
		const int16_t * blk_16 = nullptr, *blk_alt_16 = nullptr;
		const bheap::TypedBlock<slots::PrecipData *> * blk_precip = nullptr;

		switch (graph) {
			case FEELS_TEMP:
				blk_16 = *servicer.slot<int16_t *>(slots::WEATHER_TEMP_GRAPH);
				if (servicer.slot(slots::WEATHER_RTEMP_GRAPH)) blk_alt_16 = *servicer.slot<int16_t *>(slots::WEATHER_RTEMP_GRAPH);
				break;
			case REAL_TEMP:
				blk_16 = *servicer.slot<int16_t *>(slots::WEATHER_RTEMP_GRAPH);
				if (servicer.slot(slots::WEATHER_TEMP_GRAPH)) blk_alt_16 = *servicer.slot<int16_t *>(slots::WEATHER_TEMP_GRAPH);
				break;
			case WIND:
				blk_16 = *servicer.slot<int16_t *>(slots::WEATHER_WIND_GRAPH);
				if (servicer.slot(slots::WEATHER_GUST_GRAPH)) blk_alt_16 = *servicer.slot<int16_t *>(slots::WEATHER_GUST_GRAPH);
				break;
			case GUST:
				blk_16 = *servicer.slot<int16_t *>(slots::WEATHER_GUST_GRAPH);
				if (servicer.slot(slots::WEATHER_WIND_GRAPH)) blk_alt_16 = *servicer.slot<int16_t *>(slots::WEATHER_WIND_GRAPH);
				break;
			case PRECIP_HOUR:
				blk_precip = &servicer.slot<slots::PrecipData *>(slots::WEATHER_MPREC_GRAPH); break;
				break;
			case PRECIP_DAY:
				blk_precip = &servicer.slot<slots::PrecipData *>(slots::WEATHER_HPREC_GRAPH); break;
				break;
			default:
				return;
		}

		switch (graph) {
			case FEELS_TEMP:
			case REAL_TEMP:
			case WIND:
			case GUST:
				if (blk_alt_16) {
					for (uint8_t i = 0; i < 120; ++i) {
						min_ = std::min<int32_t>(min_, blk_alt_16[i]);
						max_ = std::max<int32_t>(max_, blk_alt_16[i]);
					}
				}
				for (uint8_t i = 0; i < 120; ++i) {
					min_ = std::min<int32_t>(min_, blk_16[i]);
					max_ = std::max<int32_t>(max_, blk_16[i]);
				}

				break;
			case PRECIP_HOUR:
			case PRECIP_DAY:
				for (auto precip : *blk_precip) {
					max_ = std::max<int32_t>(max_, precip.amount);
				}
				max_ = std::max<int32_t>(max_, 90); // really little amounts of rain often have high stddev and that looks really stupid
				break;
			default:
				return;
		}

		// draw axes

		int leftside = blk_precip == nullptr ? 14 : 17;
		int top = blk_precip == nullptr ? 0 : 5;
		int bottom = 64 - 14;

		int screens = graph == PRECIP_HOUR ? 5 : 4;
		int maxscroll = screens * (128 - leftside);

		if (expanded_graph_scroll > maxscroll * 128)
			expanded_graph_scroll = maxscroll * 128;

		draw_graph_xaxis_full(bottom, leftside, 128, expanded_graph_scroll / 128, maxscroll + 128 - leftside, graph != PRECIP_HOUR);
		draw_graph_yaxis(leftside, top, bottom, min_, max_, blk_precip != nullptr);

		if (blk_16) {
			draw_graph_lines(leftside + 1, top, 128, bottom, blk_16, 24, 120, min_, max_, graph < WIND, expanded_graph_scroll / 128);
		}
		else {
			draw_graph_precip(leftside + 1, top, 128, bottom, *blk_precip, (graph == PRECIP_DAY ? 24 : 12), (graph == PRECIP_DAY ? 120 : 72), (graph == PRECIP_DAY ? info->hour_precip_offset : info->minute_precip_offset), min_, max_, expanded_graph_scroll / 128);
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
			case GUST:
				txt = "wind gusts (km/h)";
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
		if (servicer.slot(slots::WEATHER_STATUS)) {
			draw_currentstats();
			draw_status();
			draw_hourlybar_header();
			if (servicer.slot(slots::WEATHER_ARRAY) && servicer.slot(slots::WEATHER_DAYS)) {
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
							slots::WEATHER_WIND_GRAPH,
							slots::WEATHER_GUST_GRAPH
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
						case GUST: servicer.set_temperature(slots::WEATHER_GUST_GRAPH, temp); break;
						case PRECIP_DAY: servicer.set_temperature(slots::WEATHER_HPREC_GRAPH, temp); break;
					}
				};

				if (ui::buttons[ui::Buttons::POWER]) { 
					servicer.set_temperature_all<
						slots::WEATHER_RTEMP_GRAPH,
						slots::WEATHER_HPREC_GRAPH,
						slots::WEATHER_WIND_GRAPH,
						slots::WEATHER_GUST_GRAPH
					>(bheap::Block::TemperatureCold);
					subscreen = MAIN;
				}
				else if (ui::buttons[ui::Buttons::SEL]) {
					show_graph_selector_timer = xTaskGetTickCount();
				}
				else if (graph_scroll_type == NO_SCROLL && ui::buttons[ui::Buttons::NXT] || ui::buttons[ui::Buttons::TAB]) {
					show_graph_selector_timer = xTaskGetTickCount();
					fix_temp(graph, bheap::Block::TemperatureWarm);
					graph = (GraphType)(1 + (int)graph);
					if (graph == MAX_GTYPE) graph = FEELS_TEMP;
					fix_temp(graph, bheap::Block::TemperatureHot);
				}
				else if (graph_scroll_type == NO_SCROLL && ui::buttons[ui::Buttons::PRV]) {
					show_graph_selector_timer = xTaskGetTickCount();
					fix_temp(graph, bheap::Block::TemperatureWarm);
					if (graph == FEELS_TEMP) graph = PRECIP_DAY;
					else graph = (GraphType)(-1 + (int)graph);
					fix_temp(graph, bheap::Block::TemperatureHot);
				}
				else if (ui::buttons[ui::Buttons::STICK]) {
					graph_scroll_type = (graph_scroll_type == NO_SCROLL) ? SCROLL_WINDOW : NO_SCROLL;
					if (graph_scroll_type == NO_SCROLL) {
						show_graph_selector_timer = xTaskGetTickCount();
					}
				}

				if (graph_scroll_type == SCROLL_WINDOW) {
					if (auto horiz = ui::buttons[ui::Buttons::X]) {
						expanded_graph_scroll += (horiz * ui::buttons.frame_time()) / 8;
					}

					show_graph_selector_timer = 0;
				}
				else if (graph_scroll_type == SCROLL_CURSOR) {
					// todo
				}

				if (expanded_graph_scroll < 0) expanded_graph_scroll = 0;

				break;
			}
		case BIG_HOURLY:
			if (int dy = ui::buttons[ui::Buttons::Y]) {
				expanded_hrbar_scroll += (dy * ui::buttons.frame_time()) / 8;
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
