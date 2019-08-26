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
	// TODO: fix me
	if (!(
		servicer.open_slot(slots::CALFIX_INFO, true, this->s_info) &&
		servicer.open_slot(slots::CALFIX_CLS1, true, this->s_c[0]) &&
		servicer.open_slot(slots::CALFIX_CLS2, true, this->s_c[1]) &&
		servicer.open_slot(slots::CALFIX_CLS3, true, this->s_c[2]) &&
		servicer.open_slot(slots::CALFIX_CLS4, true, this->s_c[3]) &&
		servicer.open_slot(slots::CALFIX_PRDH1, true, this->s_prd[0]) &&
		servicer.open_slot(slots::CALFIX_PRDH2, true, this->s_prd[1])
	)) {
		return false;
	}
	return true;
}

bool tasks::CalfixScreen::deinit() {
	// TODO: also fix me
	if (!(
		servicer.close_slot(this->s_info) &&
		servicer.close_slot(this->s_c[0]) &&
		servicer.close_slot(this->s_c[1]) &&
		servicer.close_slot(this->s_c[2]) &&
		servicer.close_slot(this->s_c[3]) &&
		servicer.close_slot(this->s_prd[0]) &&
		servicer.close_slot(this->s_prd[1])
	)) {
		return false;
	}
	ready = false;
	return true;
}

const uint16_t pos_table[4] = {
	11,
	16,
	22,
	27
};

void tasks::CalfixScreen::loop() {
	if (servicer.slot_dirty(this->s_info, true)) {
		ready = true;
	}

	if (!ready) return;

	const auto& header = servicer.slot<slots::CalfixInfo>(this->s_info);
	if (!header.active) {
		show_noschool();
		return;
	}
	draw_header(header.day, header.abnormal);
	if (strlen((char *)servicer.slot<slots::ClassInfo>(this->s_c[0]).name) < 6) return;

	for (int i = 0; i < 4; ++i) {
		const auto& a = servicer.slot<slots::PeriodInfo>(this->s_prd[i / 2]);
		draw_class((i % 2 == 0) ? a.ps1 : a.ps2, (char *)servicer.slot<slots::ClassInfo>(this->s_c[i]).name, servicer.slot<slots::ClassInfo>(this->s_c[i]).room, pos_table[i], i);
	}
}

void tasks::CalfixScreen::show_noschool() {
	// bouncy boy
	int16_t x_pos = 8 + std::round(6.5f * sinf((float)(timekeeper.current_time) / 1500.0f));
	int16_t y_pos = 20 + std::round(3.5f * sinf((float)(timekeeper.current_time) / 500.0f));

	draw::text(matrix.get_inactive_buffer(), "no school!", font::lato_bold_10::info, x_pos, y_pos, 0, 0, 255);
}

const uint8_t bg_color_table[4][3] = {
	{2, 2, 5},
	{1, 1, 2},
	{1, 2, 1},
	{2, 5, 2}
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

	if (!abnormal)
		draw::text(matrix.get_inactive_buffer(), textbuf, font::lato_bold_10::info, pos_offset - text_size / 2, 9, 244, 244, 244);
	else 
		draw::text(matrix.get_inactive_buffer(), textbuf, font::lato_bold_10::info, pos_offset - text_size / 2, 9, 244, 50, 50);
	draw::rect(matrix.get_inactive_buffer(), 0, 10, 64, 11, 1, 1, 1);

	// create the two other thingies
	uint8_t pd = day == 1 ? 4 : day - 1;
	snprintf(textbuf, 7, "<%1d", pd);
	draw::text(matrix.get_inactive_buffer(), textbuf, font::vera_8::info, 0, 8, 100, 100, 100);

	pd = day == 4 ? 1 : day + 1;
	snprintf(textbuf, 7, "%1d>", pd);
	draw::text(matrix.get_inactive_buffer(), textbuf, font::vera_8::info, 64 - draw::text_size(textbuf, font::vera_8::info) - 1, 8, 100, 100, 100);

}
