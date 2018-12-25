#include "ttc.h"
#include "timekeeper.h"
#include "fonts/tahoma_9.h"
#include "fonts/vera_7.h"
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
extern led::Matrix<led::FrameBuffer<64, 32>> matrix;
extern tasks::Timekeeper timekeeper;

void tasks::TTCScreen::loop() {
	uint16_t pos = ((timekeeper.current_time / 100) % 90) - 14;

	draw::fill(matrix.get_inactive_buffer(), 0, 0, 0);
	draw::bitmap(matrix.get_inactive_buffer(), bitmap::bus, 14, 7, 2, pos, 1, 230, 230, 230);

	draw::rect(matrix.get_inactive_buffer(), 0, 8, 64, 9, 1, 1, 1);

	uint32_t t_pos = ((timekeeper.current_time / 50));
	uint16_t size = draw::text_size("75N to S.Drive", font::tahoma_9::info);
	t_pos %= (size * 2 + 1);
	t_pos =  (size * 2 + 1) - t_pos;
	t_pos -= size;
	

	draw::text(matrix.get_inactive_buffer(), "75N to S.Drive", font::tahoma_9::info, t_pos, 16, 40, 255, 40);

	draw::rect(matrix.get_inactive_buffer(), 44, 9, 64, 32, 3, 3, 3);
	draw::text(matrix.get_inactive_buffer(), "2,2m", font::vera_7::info, 44, 15, 255, 255, 255);
	draw::rect(matrix.get_inactive_buffer(), 0, 16, 64, 17, 1, 1, 1);
}
