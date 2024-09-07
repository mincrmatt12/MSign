#include "ttc.h"
#include "../tasks/timekeeper.h"
#include "../fonts/tahoma_9.h"
#include "../fonts/lcdpixel_6.h"
#include "../srv.h"
#include "../common/slots.h"
#include "../rng.h"
#include "../draw.h"
#include "../tasks/screen.h"

#include <stdio.h>

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
extern tasks::DispMan dispman;

void screen::TTCScreen::draw() {
	// Lock the data
	srv::ServicerLockGuard g(servicer);

	// Ensure there's data
	bool ready = servicer.slot(slots::TTC_INFO); // just don't display anything

	if (ready) {
		// Alright, now we have to work out what to show
		auto& info = servicer.slot<slots::TTCInfo>(slots::TTC_INFO);
		if (servicer.slot_dirty(slots::TTC_INFO)) {
			servicer.give_lock();
			request_slots(bheap::Block::TemperatureHot);
			servicer.take_lock();
		}

		if (!dispman.interacting()) {
			auto y = scroll_helper.begin();
			// Check first slot
			for (uint8_t slot = 0; slot < 5; ++slot) {
				if (info->flags & (slots::TTCInfo::EXIST_0 << slot)) {
					y += draw_slot(y, *servicer.slot<uint8_t *>(slots::TTC_NAME_1 + slot), servicer.slot<uint64_t *>(slots::TTC_TIME_1a + slot), servicer.slot<uint64_t *>(slots::TTC_TIME_1b + slot),
						false, false, // todo
						info->altdircodes_a[slot],
						info->altdircodes_b[slot],
						info->stopdistance[slot]
					);
				}
			}
		}
		else {
			auto y = scroll_helper.begin();
			// Check first slot
			for (uint8_t slot = 0; slot < 5; ++slot) {
				if (info->flags & (slots::TTCInfo::EXIST_0 << slot)) {
					auto height = draw_slot(y, *servicer.slot<uint8_t *>(slots::TTC_NAME_1 + slot), servicer.slot<uint64_t *>(slots::TTC_TIME_1a + slot), servicer.slot<uint64_t *>(slots::TTC_TIME_1b + slot),
						false, false, // todo
						info->altdircodes_a[slot],
						info->altdircodes_b[slot],
						info->stopdistance[slot]
					);
					if (selected_slot == slot) {
						if (height) {
							draw::gradient_rect(matrix.get_inactive_buffer(), 0, y + 9, 32, y + 10, 0xaa23e8_ccu, 35_cu);
						}
						y.fix(height);
					}
					else y += height;
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

bool screen::TTCScreen::interact() {
	srv::ServicerLockGuard g(servicer);

	const auto& info = servicer.slot<slots::TTCInfo>(slots::TTC_INFO);
	if (info) {
		int dir = 0;
		if (ui::buttons[ui::Buttons::PRV]) dir -= 1;
		if (ui::buttons[ui::Buttons::NXT]) dir += 1;
		if (dir) {
			int tries = 0;
			do {  
				selected_slot += dir;
				if (selected_slot > 4) selected_slot = 0;
				if (selected_slot < 0) selected_slot = 4;
			} while (!info->exist_set(selected_slot) && ++tries < 5);
			if (tries >= 5) selected_slot = 0;
		}
	}

	return ui::buttons[ui::Buttons::POWER];
}

void screen::TTCScreen::draw_alertstr() {
	// Calculate alertstr size

	const auto& info = servicer.slot<slots::TTCInfo>(slots::TTC_INFO);

	int16_t alertstr_size = 40 + draw::text_size(*servicer[slots::TTC_ALERTSTR], font::tahoma_9::info);
	int16_t pos = 64 - (alertstr_size / 2);
	if (alertstr_size >= 120) {
		pos = draw::scroll(timekeeper.current_time / 8, alertstr_size);
	}
	if (alertstr_size >= 290 && !dispman.interacting()) {
		pos = draw::scroll(timekeeper.current_time / 6, alertstr_size);
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
			col.g -= draw::distorted_ease_wave(timekeeper.current_time, 800, 1800, 117);
			col.b += draw::distorted_ease_wave(timekeeper.current_time, 800, 1800, 10);
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

bool screen::TTCScreen::draw_subslot(uint16_t y, char dircode, const bheap::TypedBlock<uint64_t *> &times, int distance) {
	// Compute scale
	
	int min_moving_pos = 0; // rightmost place currently occupied.
	int scale_v = 8;
	int count = times.datasize / 8;

	bool drawn_anything = false;
	
	// compute scale
	for (int i = 0; i < std::min(count, 4); ++i) {
		if (times[i] < rtc_time) continue; 
		uint64_t minutes = ((times[i] - rtc_time) / 60'000);
		if (minutes > 15 && minutes < 32) scale_v = 4;
		else if (minutes > 31 && minutes < 64) scale_v = 2;
	}
	
	// positions: +7: bar, +6: text
	
	for (int i = 0; i < count; ++i) {
		if (rtc_time > times[i]) continue;

		// Compute "desired" position
		int minutes = ((times[i] - rtc_time) / 60'000);
		int position = minutes * scale_v;

		// Move if overlapping
		position = std::max(min_moving_pos, position);

		led::color_t textcolor = 0xff_c;
		if (minutes < distance) {
			textcolor = 0xff2020_cc;
		}
		else if (minutes < std::min(distance * 2 + (distance / 2), 30)) {
			textcolor = 0x70ff70_cc;
		}

		if (position < 128) drawn_anything = true;

		char buf[16] = {0};
		snprintf(buf, 16, "%dm", minutes);
		min_moving_pos = draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, position, y + 7, textcolor);
	}

	if (!drawn_anything) return false;

	// draw line
	draw::rect(matrix.get_inactive_buffer(), 0, y + 7, 128, y + 8, 38_c);

	// draw blade if necessary
	if (dircode != 0 && dircode != ' ') {
		char str[2] = {dircode, 0};
		// draw bg 
		draw::rect(matrix.get_inactive_buffer(), 128 - 6, y, 128, y + 7, 0x00556f_cc);
		// draw lines
		draw::line(matrix.get_inactive_buffer(), 128 - 6, y, 128 - 7, y + 7, 50_c);
		// draw text
		draw::text(matrix.get_inactive_buffer(), str, font::lcdpixel_6::info, 128 - 4, y + 7, 0xff_c);
	}

	return true;
}

int16_t screen::TTCScreen::draw_slot(uint16_t y, const uint8_t * name, const bheap::TypedBlock<uint64_t *> &times_a, const bheap::TypedBlock<uint64_t *> &times_b, bool alert, bool delay, char code_a, char code_b, int distance) {
	if (!name) return y;
	uint32_t t_pos = ((timekeeper.current_time / 10));
	uint16_t size = draw::text_size(name, font::tahoma_9::info);

	// If the size doesn't need scrolling, don't scroll it.
	if (size < 128) {
		t_pos = 1;
	}
	else {
		// otherwise make it scroll from the right
		t_pos = draw::scroll(t_pos, size);
	}

	int height = 0;

	if (draw_subslot(y + 9, code_a, times_a, distance)) height += 7;
	if (draw_subslot(y + 9 + height, code_b, times_b, distance)) height += 7;

	if (height) {
		draw::text(matrix.get_inactive_buffer(), name, font::tahoma_9::info, t_pos, y + 8, 255_c);
		draw::rect(matrix.get_inactive_buffer(), 0, y+9, 128, y+10, 47_c);

		height += 10;
	}

	return height;
}

void screen::TTCScreen::prepare(bool step) {
	servicer.set_temperature_all<
		slots::TTC_INFO,
		slots::TTC_ALERTSTR
	>(bheap::Block::TemperatureWarm);
	if (step) request_slots(bheap::Block::TemperatureWarm);
}

screen::TTCScreen::TTCScreen() {
	servicer.set_temperature_all<
		slots::TTC_INFO,
		slots::TTC_ALERTSTR
	>(bheap::Block::TemperatureHot);
	request_slots(bheap::Block::TemperatureHot);
	bus_type = rng::get() % 3;
	bus_type += 1;
	bus_state = ((rng::get() % 10) == 0);
	if (bus_state) {
		bus_state = (rng::get() % 2) + 1;
	}
}

void screen::TTCScreen::request_slots(uint32_t temp) {
	servicer.take_lock();
	const auto info = *servicer.slot<slots::TTCInfo>(slots::TTC_INFO);
	auto info_flags = info.flags;
	servicer.give_lock();

#define DO(x, y) if (info_flags & info.EXIST_ ## x ) servicer.set_temperature_all<slots::TTC_NAME_ ## y, slots::TTC_TIME_ ## y ## a, slots::TTC_TIME_ ## y ## b>(temp)

	// We use this format to avoid filling the request queue too much.
	DO(0, 1);
	DO(1, 2);
	DO(2, 3);
	DO(3, 4);
	DO(4, 5);

#undef DO
}

screen::TTCScreen::~TTCScreen() {
	servicer.set_temperature_all<
		slots::TTC_INFO,
		slots::TTC_TIME_1a,
		slots::TTC_TIME_2a,
		slots::TTC_TIME_3a,
		slots::TTC_TIME_4a,
		slots::TTC_TIME_5a,
		slots::TTC_TIME_1b,
		slots::TTC_TIME_2b,
		slots::TTC_TIME_3b,
		slots::TTC_TIME_4b,
		slots::TTC_TIME_5b,
		slots::TTC_NAME_1,
		slots::TTC_NAME_2,
		slots::TTC_NAME_3,
		slots::TTC_NAME_4,
		slots::TTC_NAME_5,
		slots::TTC_ALERTSTR
	>(bheap::Block::TemperatureCold);
}

void screen::TTCScreen::refresh() {
	servicer.refresh_grabber(slots::protocol::GrabberID::TRANSIT);
}
