#include "ttc.h"
#include "timekeeper.h"
#include "fonts/tahoma_9.h"
#include "fonts/vera_7.h"
#include "srv.h"
#include "common/slots.h"
#include "rng.h"
#include "draw.h"

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
}

extern uint64_t rtc_time;
extern matrix_type matrix;
extern tasks::Timekeeper timekeeper;
extern srv::Servicer servicer;

void tasks::TTCScreen::loop() {
	draw_bus();
	draw::rect(matrix.get_inactive_buffer(), 0, 8, 64, 9, 1, 1, 1);

	// Alright, now we have to work out what to show
	const slots::TTCInfo& info = servicer.slot<slots::TTCInfo>(s_info);
	if (servicer.slot_dirty(s_info, true)) ready = true;

	uint32_t y = 16;

	uint8_t name[17] = {0};

	if (ready) {
		// Check first slot
		for (uint8_t slot = 0; slot < 3; ++slot) {
			if (info.flags & (slots::TTCInfo::EXIST_0 << slot)) {
				memset(name, 0, 17);
				memcpy(name, servicer.slot(s_n[slot]), info.nameLen[slot]);
				if (
					draw_slot(y, name, servicer.slot<slots::TTCTime>(s_t[slot]).tA, servicer.slot<slots::TTCTime>(s_t[slot]).tB,
						info.flags & (slots::TTCInfo::ALERT_0 << slot),
						info.flags & (slots::TTCInfo::DELAY_0 << slot))
				) y += 8;		
			}
		}
	}
}

void tasks::TTCScreen::draw_bus() {
	uint16_t pos = ((timekeeper.current_time / 80) % 154) - 45;
	if (pos == 108) {
		bus_type = rng::get() % 3;
		bus_type += 1;
	}

	for (uint8_t i = 0; i < bus_type; ++i)
		draw::bitmap(matrix.get_inactive_buffer(), bitmap::bus, 14, 7, 2, pos + (i * 15), 1, 230, 230, 230, true);
}

bool tasks::TTCScreen::draw_slot(uint16_t y, const uint8_t * name, uint64_t time1, uint64_t time2, bool alert, bool delay) {
	uint32_t t_pos = ((timekeeper.current_time / 50));
	uint16_t size = draw::text_size(name, font::tahoma_9::info);

	// If the size doesn't need scrolling, don't scroll it.
	if (size < 43) {
		t_pos = 0;
	}
	else {
		// otherwise make it scroll from the right
		t_pos %= (size * 2) + 1;
		t_pos =  ((size * 2) + 1) - t_pos;
		t_pos -= size;
	}

	// Construct the time string.
	uint64_t next_bus = 0;
	char     time_str[64];
	if (time1 > rtc_time && time2 > rtc_time) {
		next_bus = time1;
		snprintf(time_str, 64, "%d,%dm",
					(int)((time1 - rtc_time) / 60000),
					(int)((time2 - rtc_time) / 60000)
		);
		if (draw::text_size(time_str, font::vera_7::info) > 20) {
			snprintf(time_str, 64, "%dm",
					(int)((time1 - rtc_time) / 60000)
			);
		}
	}
	else if (time1 > rtc_time) {
		next_bus = time1;
		snprintf(time_str, 64, "%dm",
				(int)((time1 - rtc_time) / 60000)
		);
	}
	else if (time2 > rtc_time) {
		next_bus = time2;
		snprintf(time_str, 64, "%dm",
				(int)((time2 - rtc_time) / 60000)
		);
	}
	else {
		return false;
	}

	// Get the color of the text, green means > 1m < 5m, red means delay/alert / >10m, white if normal but close
	if (alert || delay || (next_bus - rtc_time) > 600000) {
		draw::text(matrix.get_inactive_buffer(), name, font::tahoma_9::info, t_pos, y, 255, 20, 20);
	}
	else if ((next_bus - rtc_time) > 60000 && (next_bus - rtc_time) < 300000) {
		draw::text(matrix.get_inactive_buffer(), name, font::tahoma_9::info, t_pos, y, 40, 255, 40);
	}
	else {
		draw::text(matrix.get_inactive_buffer(), name, font::tahoma_9::info, t_pos, y, 240, 240, 240);
	}

	draw::rect(matrix.get_inactive_buffer(), 44, y - 7, 64, y+1, 3, 3, 3);
	if (delay && alert) {
		draw::rect(matrix.get_inactive_buffer(), 62, y - 7, 64, y - 3, 255, 20, 20);
		draw::rect(matrix.get_inactive_buffer(), 62, y - 3, 64, y + 1, 230, 230, 20);
	}
	else if (delay) {
		draw::rect(matrix.get_inactive_buffer(), 62, y - 7, 64, y + 1, 255, 20, 20);
	}
	else if (alert) {
		draw::rect(matrix.get_inactive_buffer(), 62, y - 7, 64, y + 1, 230, 230, 20);
	}
	draw::text(matrix.get_inactive_buffer(), time_str, font::vera_7::info, 44, y-1, 255, 255, 255);
	draw::rect(matrix.get_inactive_buffer(), 0, y, 64, y + 1, 1, 1, 1);

	return true;
}

bool tasks::TTCScreen::init() {
	// TODO: fix me
	if (!(
		servicer.open_slot(slots::TTC_INFO, true, this->s_info) &&
		servicer.open_slot(slots::TTC_NAME_1, true, this->s_n[0]) &&
		servicer.open_slot(slots::TTC_NAME_2, true, this->s_n[1]) &&
		servicer.open_slot(slots::TTC_NAME_3, true, this->s_n[2]) &&
		servicer.open_slot(slots::TTC_TIME_1, true, this->s_t[0]) &&
		servicer.open_slot(slots::TTC_TIME_2, true, this->s_t[1]) &&
		servicer.open_slot(slots::TTC_TIME_3, true, this->s_t[2])
	)) {
		return false;
	}
	return true;
}

bool tasks::TTCScreen::deinit() {
	// TODO: also fix me
	if (!(
		servicer.close_slot(this->s_info) &&
		servicer.close_slot(this->s_n[0]) &&
		servicer.close_slot(this->s_n[1]) &&
		servicer.close_slot(this->s_n[2]) &&
		servicer.close_slot(this->s_t[0]) &&
		servicer.close_slot(this->s_t[1]) &&
		servicer.close_slot(this->s_t[2])
	)) {
		ready = false;
		return false;
	}
	return true;
}
