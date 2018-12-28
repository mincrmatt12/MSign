#include "clock.h"
#include "rng.h"
#include <time.h>

#include "fonts/latob_12.h"
#include "draw.h"

extern uint64_t rtc_time;
extern led::Matrix<led::FrameBuffer<64, 32>> matrix;

bool tasks::ClockScreen::init() {
	name[0] = 'c';
	name[1] = 'k';
	name[2] = 's';
	name[3] = 'n';

	bg_color[0] = rng::get() % 256;
	bg_color[1] = rng::get() % 256;
	bg_color[2] = rng::get() % 256;

	return true;
}

void tasks::ClockScreen::loop() {
	struct tm timedat;
	time_t now = rtc_time / 1000;
	char buf[6];
	gmtime_r(&now, &timedat);

	snprintf(buf, 6, "%02d:%02d", timedat.tm_hour, timedat.tm_min);

	draw::fill(matrix.get_inactive_buffer(), bg_color[0], bg_color[1], bg_color[2]);
	uint16_t text_size = draw::text_size((uint8_t *)buf, font::lato_bold_12::info);
	uint16_t box_width = text_size + 2;

	draw::rect(matrix.get_inactive_buffer(), (32 - box_width / 2), 10, (32 + box_width / 2 + 1), 22, 0, 0, 0);
	draw::text(matrix.get_inactive_buffer(), buf, font::lato_bold_12::info, (32 - text_size / 2), 21, 240, 240, 240);
}
