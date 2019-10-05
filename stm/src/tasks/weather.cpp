#include "weather.h"
#include "srv.h"
#include "tasks/timekeeper.h"
#include "common/slots.h"
#include "fonts/latob_15.h"
#include "fonts/dejavu_10.h"
#include "fonts/tahoma_9.h"

#include "draw.h"

#include <cmath>
extern uint64_t rtc_time;
extern matrix_type matrix;
extern tasks::Timekeeper timekeeper;
extern srv::Servicer servicer;

namespace bitmap {
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

bool tasks::WeatherScreen::init() {
	// TODO: fix me
	if (!(
		servicer.open_slot(slots::WEATHER_INFO, true, this->s_info) &&
		servicer.open_slot(slots::WEATHER_ICON, true, this->s_icon) &&
		servicer.open_slot(slots::WEATHER_ARRAY1, true, this->s_state[0]) &&
		servicer.open_slot(slots::WEATHER_ARRAY2, true, this->s_state[1]) &&
		s_status.open(slots::WEATHER_STATUS) &&
		s_tempgraph.open(slots::WEATHER_TEMP_GRAPH)
	)) {
		return false;
	}
	return true;
}

bool tasks::WeatherScreen::deinit() {
	// TODO: fix me
	s_status.close();
	s_tempgraph.close();
	if (!(
		servicer.close_slot(s_info) &&
		servicer.close_slot(s_icon) &&
		servicer.close_slot(s_state[0]) &&
		servicer.close_slot(s_state[1])
	)) {
		return false;
	}
	return true;
}

void tasks::WeatherScreen::draw_currentstats() {
	char disp_buf[16];
	float ctemp = servicer.slot<slots::WeatherInfo>(s_info).ctemp;
	snprintf(disp_buf, 16, "%.01f", ctemp);

	uint16_t text_size;

	text_size = draw::text_size(disp_buf, font::lato_bold_15::info);

	if (ctemp > 0.0 && ctemp < 16.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 240, 240, 240);
	}
	else if (ctemp < 0.0 && ctemp > -10.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 100, 100, 240);
	}
	else if (ctemp >= 16.0 && ctemp < 30.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 50, 240, 50);
	}
	else if (ctemp > 30.0) {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 250, 30, 30);
	}
	else {
		draw::text(matrix.get_inactive_buffer(), disp_buf, font::lato_bold_15::info, 44 - text_size / 2, 12, 40, 40, 255);
	}

	snprintf(disp_buf, 16, "%.0f", servicer.slot<slots::WeatherInfo>(s_info).ltemp);
	draw::text(matrix.get_inactive_buffer(), disp_buf, font::dejavusans_10::info, 22, 20, 127, 127, 127);

	snprintf(disp_buf, 16, "%.0f", servicer.slot<slots::WeatherInfo>(s_info).htemp);
	text_size = draw::text_size(disp_buf, font::dejavusans_10::info);
	draw::text(matrix.get_inactive_buffer(), disp_buf, font::dejavusans_10::info, 63 - text_size, 20, 127, 127, 127);

	int16_t y = (int16_t)(std::round(4.5f * sinf((float)(timekeeper.current_time) / 700.0f) + 5));

	const char * icon = (const char*)servicer.slot(s_icon);

	if (strcmp(icon, "cloudy") == 0 || strcmp(icon, "partly-cloudy") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::cloudy, 20, 20, 3, 1, y, 235, 235, 235);
	}
	else if (strcmp(icon, "sleet") == 0 || strcmp(icon, "snow") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::snow, 20, 20, 3, 1, y, 255, 255, 255);
	}
	else if (strcmp(icon, "clear-day") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::clear_day, 20, 20, 3, 1, y, 252, 252, 17);
	}
	else if (strcmp(icon, "clear-night") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::night, 20, 20, 3, 1, y, 79, 78, 79);
	}
	else if (strcmp(icon, "wind") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::wind, 20, 20, 3, 1, y, 118, 118, 118);
	}
	else if (strcmp(icon, "fog") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::fog, 20, 20, 3, 1, y, 118, 118, 118);
	}
	else if (strcmp(icon, "rain") == 0) {
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::rain, 20, 20, 3, 1, y, 43, 182, 255);
	}
	else {
		if (icon[15] != 0 && icon[14] == 'd') {
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::cloudy, 20, 20, 3, 1, y, 245, 245, 245);
		}
		else if (icon[15] != 0) {
			draw::bitmap(matrix.get_inactive_buffer(), bitmap::cloudy, 20, 20, 3, 1, y, 79, 79, 79);
		}
	}
}

void tasks::WeatherScreen::draw_status() {
	if (s_status.data) {
		uint16_t text_size = draw::text_size(s_status.data, font::tahoma_9::info);
		if (text_size < 128) {
			draw::text(matrix.get_inactive_buffer(), s_status.data, font::tahoma_9::info, 32 - text_size / 2, 29, 240, 240, 240);
		}
		else {
			uint16_t t_pos = (uint16_t)(timekeeper.current_time / (40 - (strlen(s_status.data) / 7)));
			t_pos %= (text_size * 2) + 1;
			t_pos =  ((text_size * 2) + 1) - t_pos;
			t_pos -= text_size;
			draw::text(matrix.get_inactive_buffer(), s_status.data, font::tahoma_9::info, t_pos, 61, 240, 240, 240);
		}
	}
}

void tasks::WeatherScreen::draw_hourlybar_header() {
	// bar starts at x = 4, ends at x = 124, every 5 steps = 1 thing
	// bar takes 13 px vertically, starts at y = 53
	
	int first = (rtc_time % (24*60*60*1000)) / 60*60*1000;

	for (int i = 0; i < 24 / 4; ++i) {
		int hour = (first + i * 4) % 24;
		char buf[3] = {0};
		snprintf(buf, 3, "%2d", hour);

		// todo  make this a gradient
		draw::text(matrix.get_inactive_buffer(), buf, 4 + i * 20, 38, 255, 255, 255);
	}
}

void tasks::WeatherScreen::draw_hourlybar(uint8_t hour) {
	int start = 4 + hour * 20;
	int end =   9 + hour * 20;
}

void tasks::WeatherScreen::loop() {
	s_status.update();
	s_tempgraph.update();
	if (servicer.slot_dirty(s_info, true)) {
		s_status.renew();
		s_tempgraph.renew();
	}
	
	if (done()) progress = 0;
	switch (progress) {
		case 0:
			draw_currentstats();
			break;
		case 1:
			draw_status();
			break;
		case 2:
			draw_hourlybar_header();
			break;
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
			draw_hourlybar(progress - 3);
			break;
	}

	++progress;
}
