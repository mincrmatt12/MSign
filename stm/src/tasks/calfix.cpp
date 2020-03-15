#include "calfix.h"
#include "srv.h"
#include "tasks/timekeeper.h"
#include "common/slots.h"
#include "fonts/latob_15.h"
#include "fonts/lcdpixel_6.h"
#include "fonts/tahoma_9.h"
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
	16,
	25,
	37,
	46
};

void tasks::CalfixScreen::draw_dayp(uint16_t start, uint16_t end, uint16_t lpos, char indicator, uint64_t ps, uint64_t pe) {
	char buf[2] = {indicator, 0};
	uint64_t daytime = rtc_time % (24 * 60 * 60 * 1000);
	if (daytime >= ps) {
		if (daytime < pe) {
			float offset = end - start;
			float diff =   pe - ps;

			uint16_t bpos = ((daytime - ps) / diff) * offset;
			bpos += start;

			draw::rect(matrix.get_inactive_buffer(), start, 55, bpos, 64, 24_c, 35_c, 24_c);
			draw::text(matrix.get_inactive_buffer(), buf, font::tahoma_9::info, lpos, 63, 100_c, 4095, 100_c);
		}
		else {
			draw::rect(matrix.get_inactive_buffer(), start, 55, end,  64, 24_c, 24_c, 24_c);
			draw::text(matrix.get_inactive_buffer(), buf, font::tahoma_9::info, lpos, 63, 4095, 4095, 4095);
		}
	}
	else {
		draw::text(matrix.get_inactive_buffer(), buf, font::tahoma_9::info, lpos, 63, 4095, 50_c, 50_c);
	}
}

void tasks::CalfixScreen::loop() {
	if (servicer.slot_dirty(this->s_info, true)) {
		ready = true;
	}

	if (progress == 5) progress = 0;

	if (!ready) {
		progress = 5;
		return;
	}

	const auto& header = servicer.slot<slots::CalfixInfo>(this->s_info);
	if (!header.active) {
		show_noschool();
		progress = 5;
		return;
	}

	if (strlen((char *)servicer.slot<slots::ClassInfo>(this->s_c[0]).name) < 6) {
		progress = 5;
		return;
	}

	switch (progress) {
	case 0:
		draw_header(header.day, header.abnormal);
		progress = 1;
		return;
	case 1:
	case 2:
		for (int i = (progress == 1 ? 0 : 2); i < (progress == 1 ? 2 : 4); ++i) {
			const auto& a = servicer.slot<slots::PeriodInfo>(this->s_prd[i / 2]);
			draw_class((i % 2 == 0) ? a.ps1 : a.ps2, (char *)servicer.slot<slots::ClassInfo>(this->s_c[i]).name, servicer.slot<slots::ClassInfo>(this->s_c[i]).room, pos_table[i], i);
		}
		++progress;
		return;
	case 3:
	case 4:
		const auto& ps23 = servicer.slot<slots::PeriodInfo>(this->s_prd[1]);
		const auto& ps01 = servicer.slot<slots::PeriodInfo>(this->s_prd[0]);

		uint64_t clslength = ps23.ps2 - ps23.ps1 - 5 * 60 * 1000;
		uint64_t clstlength = clslength + 5 * 60 * 1000;
		if (ps01.ps2 + clslength + 5 * 60 * 1000 != ps23.ps1) {
			if (progress == 3) {
				draw_dayp(0, 24, 10, '1', ps01.ps1, ps01.ps1 + clstlength);
				draw_dayp(24, 48, 34, '2', ps01.ps2, ps01.ps2 + clslength);
				draw_dayp(48, 72, 58, 'L', ps01.ps2 + clslength, ps23.ps1);
			}
			else {
				draw_dayp(72, 94, 82, '3', ps23.ps1, ps23.ps1 + clstlength);
				draw_dayp(94, 128, 108, '4', ps23.ps2, ps23.ps2 + clslength);
			}
		}
		++progress;
		return;
	}
}

void tasks::CalfixScreen::show_noschool() {
	// bouncy boy
	int16_t x_pos = 27 + std::round(12.0f * sinf((float)(timekeeper.current_time) / 1300.0f));
	int16_t y_pos = 38 + std::round(6.7f * sinf((float)(timekeeper.current_time) / 400.0f));

	draw::text(matrix.get_inactive_buffer(), "no school!", font::lato_bold_15::info, x_pos, y_pos, 0, 0, 4095);
}

const uint16_t bg_color_table[4][3] = {
	{12, 8, 16},
	{8, 7, 12},
	{7, 12, 8},
	{12, 16, 12}
};

const uint16_t cls_fg_color_table[4][3] = {
	{4095, 180_c, 1_c},
	{200_c, 200_c, 100_c},
	{50_c,  50_c, 4095},
	{4095, 4095, 4095}
};

void tasks::CalfixScreen::draw_class(uint64_t clstm, const char * cname, uint16_t room, uint16_t y_pos, uint8_t cl) {
	if (!cname) return;
	
	draw::rect(matrix.get_inactive_buffer(), 0, y_pos, 64, y_pos + 9, bg_color_table[cl][0], bg_color_table[cl][1], bg_color_table[cl][2]);
	draw::text(matrix.get_inactive_buffer(), cname, font::tahoma_9::info, 1, y_pos + 8, cls_fg_color_table[cl][0], cls_fg_color_table[cl][1], cls_fg_color_table[cl][2]);

	draw::rect(matrix.get_inactive_buffer(), 0, y_pos, 64, y_pos + 1, bg_color_table[cl][0] + 50, bg_color_table[cl][1] + 50, bg_color_table[cl][2] + 50);
	draw::rect(matrix.get_inactive_buffer(), 0, y_pos + 8, 128, y_pos + 9, bg_color_table[cl][0] + 50, bg_color_table[cl][1] + 50, bg_color_table[cl][2] + 50);

	char text_buffer[32] = {0};
	struct tm timedat;
	time_t instant = clstm / 1000;
	gmtime_r(&instant, &timedat);
	snprintf(text_buffer, 32, "rm %03d / %02d:%02d", room, timedat.tm_hour, timedat.tm_min);

	uint16_t text_size = draw::text_size(text_buffer, font::lcdpixel_6::info);
	draw::text(matrix.get_inactive_buffer(), text_buffer, font::lcdpixel_6::info, 128 - text_size - 1, y_pos + 7, 4095, 4095, 4095);
}

void tasks::CalfixScreen::draw_header(uint8_t day, bool abnormal) {
	char textbuf[7];

	snprintf(textbuf, 7, "Day %1d", day);
	uint16_t text_size = draw::text_size(textbuf, font::lato_bold_15::info);
	int16_t pos_offset = 64 + std::round(7.5f * sinf((float)(timekeeper.current_time) / 1000.0f));

	if (!abnormal)
		draw::text(matrix.get_inactive_buffer(), textbuf, font::lato_bold_15::info, pos_offset - text_size / 2, 12, 244_c, 244_c, 244_c);
	else 
		draw::text(matrix.get_inactive_buffer(), textbuf, font::lato_bold_15::info, pos_offset - text_size / 2, 12, 244_c, 50_c, 50_c);
	draw::rect(matrix.get_inactive_buffer(), 0, 15, 128, 16, 1_c, 1_c, 1_c);

	// create the two other thingies
	uint8_t pd = day == 1 ? 4 : day - 1;
	snprintf(textbuf, 7, "<%1d", pd);
	draw::text(matrix.get_inactive_buffer(), textbuf, font::vera_8::info, 0, 10, 100_c, 100_c, 100_c);

	pd = day == 4 ? 1 : day + 1;
	snprintf(textbuf, 7, "%1d>", pd);
	draw::text(matrix.get_inactive_buffer(), textbuf, font::vera_8::info, 128 - draw::text_size(textbuf, font::vera_8::info) - 1, 10, 100_c, 100_c, 100_c);

}
