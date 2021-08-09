#include "ttc.h"
#include "../tasks/timekeeper.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/lcdpixel_6.h"
#include "../srv.h"
#include "../common/slots.h"
#include "../rng.h"
#include "../draw.h"

namespace bitmap {
	const uint8_t bus[] = { // really shitty bus
		0b11111111, 0b11111100,
		0b10100010, 0b00100100,
		0b10100010, 0b00100100,
		0b10111111, 0b11111100,
		0b10111111, 0b11111100,
		0b11101111, 0b10110000,
		0b00010000, 0b01000000
	}; // stride = 2, width = 14, height = 7

	const uint8_t subway[] = {
		0b11111111,0b11111111,0b00000000,
		0b10000001,0b11110000,0b10000000,
		0b10111101,0b01010110,0b10000000,
		0b10111101,0b01010110,0b01000000,
		0b10000001,0b01010000,0b01000000,
		0b11111111,0b11111111,0b11000000,
		0b11010110,0b00001101,0b01000000,
		0b00101000,0b00000010,0b10000000
	}; // w=18, h=8, stride=3
}

extern uint64_t rtc_time;
extern matrix_type matrix;
extern tasks::Timekeeper timekeeper;
extern srv::Servicer servicer;

void screen::TTCScreen::draw() {
	// Lock the data
	srv::ServicerLockGuard g(servicer);

	// Ensure there's data
	bool ready = servicer.slot(slots::TTC_INFO); // just don't display anything

	if (ready) {

		// Alright, now we have to work out what to show
		const auto& info = servicer.slot<slots::TTCInfo>(slots::TTC_INFO);

		int16_t y = 10;
		if (((~info->flags) & (slots::TTCInfo::SUBWAY_ALERT | slots::TTCInfo::EXIST_0 | slots::TTCInfo::EXIST_1 | slots::TTCInfo::EXIST_2)) == 0) {
			y = 10 - draw::distorted_ease_wave(rtc_time, 1000, 4500, 18);
		}

		// Check first slot
		for (uint8_t slot = 0; slot < 3; ++slot) {
			if (info->flags & (slots::TTCInfo::EXIST_0 << slot)) {
				if (!servicer.slot(slots::TTC_TIME_1 + slot)) continue;
				if (draw_slot(y, *servicer.slot<uint8_t *>(slots::TTC_NAME_1 + slot), *servicer.slot<uint64_t *>(slots::TTC_TIME_1 + slot), 
							info->flags & (slots::TTCInfo::ALERT_0 << slot),
							info->flags & (slots::TTCInfo::DELAY_0 << slot)
							)) {
					y += 18;
				}
			}
		}
	}

	draw::rect(matrix.get_inactive_buffer(), 0, 0, 128, 10, 0);
	if (!ready || (!(servicer.slot<slots::TTCInfo>(slots::TTC_INFO)->flags & slots::TTCInfo::SUBWAY_ALERT))) {
		draw_bus();
	}
	else if (servicer.slot(slots::TTC_ALERTSTR)) {
		draw_alertstr();
	}
	draw::rect(matrix.get_inactive_buffer(), 0, 9, 128, 10, 50_c);

}

void screen::TTCScreen::draw_alertstr() {
	// Calculate alertstr size

	const auto& info = servicer.slot<slots::TTCInfo>(slots::TTC_INFO);

	int16_t alertstr_size = 40 + draw::text_size(*servicer[slots::TTC_ALERTSTR], font::tahoma_9::info);
	int16_t pos = 64 - (alertstr_size / 2);
	if (alertstr_size >= 120) {
		pos = draw::scroll(rtc_time / 8, alertstr_size);
	}
	if (alertstr_size >= 290) {
		pos = draw::scroll(rtc_time / 6, alertstr_size);
	}

	led::color_t col(4095);
	bool f = false;
	if (info->flags & slots::TTCInfo::SUBWAY_DELAYED) {
		f = true;
		col.g = 127_c;
		col.b = 0;
	}
	if (info->flags & slots::TTCInfo::SUBWAY_OFF) {
		if (!f) {
			col.g = 10;
			col.b = 10;
		}
		else {
			col.g -= draw::distorted_ease_wave(rtc_time, 800, 1800, 117);
			col.b += draw::distorted_ease_wave(rtc_time, 800, 1800, 10);
		}
	}

	draw::bitmap(matrix.get_inactive_buffer(), bitmap::subway, 18, 8, 3, pos, 1, 0xff_c, true);
	draw::bitmap(matrix.get_inactive_buffer(), bitmap::subway, 18, 8, 3, draw::text(matrix.get_inactive_buffer(), *servicer[slots::TTC_ALERTSTR], font::tahoma_9::info, pos + 20, 8, col) + 2, 1, 0xff_c);
}

void screen::TTCScreen::draw_bus() {
	uint16_t pos = ((timekeeper.current_time / 60) % 228) - 45;
	if (pos == 227 - 45) {
		bus_type = rng::get() % 3;
		bus_type += 1;
	}

	switch (bus_state) {
		case 0:
			for (uint8_t i = 0; i < bus_type; ++i)
				draw::bitmap(matrix.get_inactive_buffer(), bitmap::bus, 14, 7, 2, pos + (i * 24), 2, 230_c, true);
			break;
		case 2:
			pos += 45;
			pos = (pos * pos) % 228;
			pos -= 45;
			[[fallthrough]];
		case 1:
			{
				for (uint8_t i = 0; i < bus_type + (bus_state == 2 ? 2 : 0); ++i)
					draw::bitmap(matrix.get_inactive_buffer(), bitmap::bus, 14, 7, 2, pos + (i * 24), 2, {rng::getclr(), rng::getclr(), rng::getclr()}, true);
			}
			break;
	}

}

bool screen::TTCScreen::draw_slot(uint16_t y, const uint8_t * name, const uint64_t times[6], bool alert, bool delay) {
	if (!name) return false;
	uint32_t t_pos = ((timekeeper.current_time / 50));
	uint16_t size = draw::text_size(name, font::tahoma_9::info);

	// If the size doesn't need scrolling, don't scroll it.
	if (size < 128) {
		t_pos = 1;
	}
	else {
		// otherwise make it scroll from the right
		t_pos = draw::scroll(t_pos, size);
	}

	int16_t write_pos[6];
	memset(write_pos, 0xff, sizeof write_pos);
	int16_t min_pos = 0;
	int64_t scale_v = 8;

	// Only consider the first 4 entries for auto-scaling
	for (int i = 0; i < 4; ++i) {
		if (times[i] < rtc_time) continue;
		uint64_t minutes = ((times[i] - rtc_time) / 60'000);
		if (minutes > 15 && minutes < 32) scale_v = 4;
		else if (minutes > 31 && minutes < 64) scale_v = 2;
	}

	for (int i = 0; i < 6; ++i) {
		if (rtc_time > times[i]) continue;

		// Scale is 8 pixels per minute
		int16_t position = static_cast<int16_t>((times[i] - rtc_time) / (60'000 / scale_v));

		if (position > 128) {
			break;
		}

		if (position < min_pos) {
			position = min_pos;
		}

		write_pos[i] = position + 2;
		char buf[16] = {0};
		uint64_t minutes = ((times[i] - rtc_time) / 60'000);
		snprintf(buf, 16, "%dm", (int)minutes);

		min_pos = position + draw::text_size(buf, font::lcdpixel_6::info);
	}

	draw::text(matrix.get_inactive_buffer(), name, font::tahoma_9::info, t_pos, y + 8, 255_c);
	draw::rect(matrix.get_inactive_buffer(), 0, y+9, 128, y+10, 20_c);

	for (int i = 0; i < 6; ++i) {
		if (write_pos[i] < 0) continue;
		if (times[i] < rtc_time) continue;

		char buf[16] = {0};
		uint64_t minutes = ((times[i] - rtc_time) / 60'000);
		if (minutes > 63) break;
		snprintf(buf, 16, "%dm", (int)minutes);

		if (minutes < 5) {
			draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, write_pos[i], y+16, 255_c);
		}
		else if (minutes < 13) {
			draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, write_pos[i], y+16, {100_c, 255_c, 100_c});
		}
		else {
			draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, write_pos[i], y+16, {255_c, 70_c, 70_c});
		}
	}

	draw::rect(matrix.get_inactive_buffer(), 0, y+16, 128, y+17, 50_c);

	return true;
}

void screen::TTCScreen::prepare(bool) {
	servicer.set_temperature_all<
		slots::TTC_INFO,
		slots::TTC_NAME_1,
		slots::TTC_NAME_2,
		slots::TTC_NAME_3,
		slots::TTC_TIME_1,
		slots::TTC_TIME_2,
		slots::TTC_TIME_3,
		slots::TTC_ALERTSTR
	>(bheap::Block::TemperatureWarm);
}

screen::TTCScreen::TTCScreen() {
	servicer.set_temperature_all<
		slots::TTC_INFO,
		slots::TTC_NAME_1,
		slots::TTC_NAME_2,
		slots::TTC_NAME_3,
		slots::TTC_TIME_1,
		slots::TTC_TIME_2,
		slots::TTC_TIME_3,
		slots::TTC_ALERTSTR
	>(bheap::Block::TemperatureHot);
	bus_type = rng::get() % 3;
	bus_type += 1;
	bus_state = ((rng::get() % 10) == 0);
	if (bus_state) {
		bus_state = (rng::get() % 2) + 1;
	}
}

screen::TTCScreen::~TTCScreen() {
	servicer.set_temperature_all<
		slots::TTC_INFO,
		slots::TTC_NAME_1,
		slots::TTC_NAME_2,
		slots::TTC_NAME_3,
		slots::TTC_TIME_1,
		slots::TTC_TIME_2,
		slots::TTC_TIME_3,
		slots::TTC_ALERTSTR
	>(bheap::Block::TemperatureCold);
}
