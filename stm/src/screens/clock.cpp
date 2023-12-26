#include "clock.h"
#include "../rng.h"
#include <stdio.h>
#include <time.h>

#include "../fonts/dejavu_12.h"
#include "../draw.h"

extern uint64_t rtc_time;
extern matrix_type matrix;

screen::ClockScreen::ClockScreen() {
	bg_color.r = std::max(rng::getclr(), 50_c);
	bg_color.g = std::max(rng::getclr(), 50_c);
	bg_color.b = std::max(rng::getclr(), 50_c);
}

void screen::ClockScreen::draw() {
	struct tm timedat;
	time_t now = rtc_time / 1000;
	char buf[6];
	gmtime_r(&now, &timedat);

	snprintf(buf, 6, "%02d:%02d", timedat.tm_hour, timedat.tm_min);

	uint16_t width = draw::text_size(buf, font::dejavusans_12::info);
	draw::outline_text(matrix.get_inactive_buffer(), buf, font::dejavusans_12::info, 60 - (width / 2) + draw::distorted_ease_wave(rtc_time, 150, 850, 8), 11, bg_color);
}
