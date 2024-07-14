#include "clock.h"
#include "../rng.h"
#include "../mintime.h"

#include "../fonts/dejavu_12.h"
#include "../draw.h"

#include <stdio.h>

extern uint64_t rtc_time;
extern matrix_type matrix;

screen::ClockScreen::ClockScreen() {
	bg_color.r = std::max(rng::getclr(), 50_c);
	bg_color.g = std::max(rng::getclr(), 50_c);
	bg_color.b = std::max(rng::getclr(), 50_c);
}

void screen::ClockScreen::draw() {
	char buf[6];
	auto timedat = mint::now();

	snprintf(buf, 6, "%02d:%02d", timedat.tm_hour, timedat.tm_min);

	uint16_t width = draw::text_size(buf, font::dejavusans_12::info);
	draw::outline_text(matrix.get_inactive_buffer(), buf, font::dejavusans_12::info, 60 - (width / 2) + draw::distorted_ease_wave(rtc_time, 150, 850, 8), 11, bg_color);
}
