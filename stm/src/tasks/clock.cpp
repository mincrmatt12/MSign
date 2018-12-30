#include "clock.h"
#include "rng.h"
#include <time.h>

#include "fonts/tahoma_9.h"
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

	uint16_t width = draw::text_size(buf, font::tahoma_9::info);
	//draw::rect(matrix.get_inactive_buffer(), (32 - box_width / 2), 10, (32 + box_width / 2 + 1), 22, 0, 0, 0);
	draw::text(matrix.get_inactive_buffer(), buf, font::tahoma_9::info, 32 - (width / 2), 7, 240, 240, 240);
}
