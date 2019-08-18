#include "calfix.h"
#include "srv.h"
#include "tasks/timekeeper.h"
#include "common/slots.h"
#include "fonts/latob_10.h"
#include "fonts/vera_7.h"
#include "fonts/vera_8.h"
#include <time.h>

#include "../draw.h"
#include <cmath>

extern uint64_t rtc_time;
extern matrix_type matrix;
extern tasks::Timekeeper timekeeper;
extern srv::Servicer servicer;

bool tasks::CalfixScreen::init() {
	return true; // TODO
}

bool tasks::CalfixScreen::deinit() {
	return true; // TODO
}

void tasks::CalfixScreen::loop() {
	draw_header(1, false);
	// test content to see how the screen might look
	draw_class(0, "CHC2D3-02", 203, 11, 0);
	draw_class(1040000, "ENG3U3-02", 203, 16, 1);
	draw_class(1040000, "SPH3U3-01", 204, 22, 2);
	draw_class(1040000, "MPM4U0-02", 204, 27, 3);
}

const uint8_t bg_color_table[4][3] = {
	{2, 2, 5},
	{1, 1, 2},
	{1, 2, 1},
	{2, 4, 2}
};

const uint8_t cls_fg_color_table[4][3] = {
	{255, 140, 1},
	{200, 190, 100},
	{50, 60, 255},
	{255, 255, 255}
};

void tasks::CalfixScreen::draw_class(uint64_t clstm, const char * cname, uint16_t room, uint16_t y_pos, uint8_t cl) {
	if (!cname) return;
	draw::rect(matrix.get_inactive_buffer(), 0, y_pos, 64, y_pos + 5, bg_color_table[cl][0], bg_color_table[cl][1], bg_color_table[cl][2]);
	draw::rect(matrix.get_inactive_buffer(), 0, y_pos + 5, 64, y_pos + 6, 30, 30, 30);

	draw::text(matrix.get_inactive_buffer(), cname, font::vera_7::info, 0, y_pos + 5, cls_fg_color_table[cl][0], cls_fg_color_table[cl][1], cls_fg_color_table[cl][2]);

	char textbuf[7] = {0};

	if (rtc_time % 8000 > 5000) {
		snprintf(textbuf, 7, "rm%03d", room);
	}
	else {
		struct tm timedat;
		time_t now = clstm / 1000;
		gmtime_r(&now, &timedat);

		snprintf(textbuf, 7, "%02d:%02d", timedat.tm_hour, timedat.tm_min);
	}

	uint16_t length = draw::text_size(textbuf, font::vera_7::info);
	draw::rect(matrix.get_inactive_buffer(), 64 - length - 1, y_pos, 64, y_pos + 5, 0, 0, 0);
	draw::text(matrix.get_inactive_buffer(), textbuf, font::vera_7::info, 64 - length - 1, y_pos + 5, 255, 255, 255);
}

void tasks::CalfixScreen::draw_header(uint8_t day, bool abnormal) {
	char textbuf[7];

	snprintf(textbuf, 7, "Day %1d", day);
	uint16_t text_size = draw::text_size(textbuf, font::lato_bold_10::info);
	int16_t pos_offset = 32 + std::round(5.5f * sinf((float)(timekeeper.current_time) / 1000.0f));

	draw::text(matrix.get_inactive_buffer(), textbuf, font::lato_bold_10::info, pos_offset - text_size / 2, 9, 244, 244, 244);
	draw::rect(matrix.get_inactive_buffer(), 0, 10, 64, 11, 1, 1, 1);

	// create the two other thingies
	uint8_t pd = day == 1 ? 4 : day - 1;
	snprintf(textbuf, 7, "<%1d", pd);
	draw::text(matrix.get_inactive_buffer(), textbuf, font::vera_8::info, 0, 8, 100, 100, 100);

	pd = day == 4 ? 1 : day + 1;
	snprintf(textbuf, 7, "%1d>", pd);
	draw::text(matrix.get_inactive_buffer(), textbuf, font::vera_8::info, 64 - draw::text_size(textbuf, font::vera_8::info) - 1, 8, 100, 100, 100);

}
