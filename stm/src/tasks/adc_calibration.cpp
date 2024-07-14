#include "screen.h"

#include "../ui.h"

#include "../fonts/tahoma_9.h"
#include "../fonts/lcdpixel_6.h"
#include "../srv.h"
#include "../draw.h"
#include "timekeeper.h"

#include <stdio.h>

extern srv::Servicer servicer;
extern matrix_type matrix;
extern tasks::Timekeeper timekeeper;

void tasks::DispMan::do_adc_calibration() {
	enum Phase : int {
		CAPTURE_MIN_MAX,
		CAPTURE_DOWN,
		CAPTURE_LEFT,
		MOVE_CENTER,
		CAPTURE_DEADZONE,
		VISUALIZE
	} phase = CAPTURE_MIN_MAX;

	static const char * const titles[] = {
		"Move stick over",
		"Move stick down",
		"Move stick left",
		"Center stick",
		"Leave still",
		"Test results"
	};
	static const char * const titles2[] = {
		"full range",
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr
	};

	// adc is from 0-8192
	uint16_t x, y; ui::buttons.read_raw_adc(x, y);
	slots::protocol::AdcCalibration new_calibration {
		.x = {
			.min = x,
			.max = x,
			.deadzone = 0,
		},
		.y = {
			.min = y,
			.max = y,
			.deadzone = 0
		}
	};

	while (true) {
		matrix.get_inactive_buffer().clear();
		ui::buttons.read_raw_adc(x, y);
		ui::buttons.update();
		// Draw current ADC position
		{
			uint16_t px = 64 + x / 64,
					 py =      y / 64;
			draw::circle(matrix.get_inactive_buffer(), px-2, py-2, px+3, py+3, 0x2222ff_cc);
		}
		if (phase == VISUALIZE) {
			uint16_t px = 96 + ui::Buttons::adjust_adc_value(x, new_calibration.x) / 4,
					 py = 32 + ui::Buttons::adjust_adc_value(y, new_calibration.y) / 4;
			draw::circle(matrix.get_inactive_buffer(), px-2, py-2, px+3, py+3, 0x22ff22_cc);
		}
		draw::outline(matrix.get_inactive_buffer(), 64, 0, 128, 64, 0xff_c);
		if (phase == CAPTURE_MIN_MAX) {
			draw::outline(matrix.get_inactive_buffer(), 
				64 + new_calibration.x.min / 64,
				     new_calibration.y.min / 64,
				64 + new_calibration.x.max / 64,
				     new_calibration.y.max / 64, 0x2222ff_cc);
		}
		else if (phase == CAPTURE_DEADZONE) {
			int centerx = new_calibration.x.min + new_calibration.x.max;
			int centery = new_calibration.y.min + new_calibration.y.max;
			centerx /= 128;
			centery /= 128;
			centerx += 64;
			draw::outline(matrix.get_inactive_buffer(),
				centerx - new_calibration.x.deadzone / 64,
				centery - new_calibration.y.deadzone / 64,
				centerx + new_calibration.x.deadzone / 64,
				centery + new_calibration.y.deadzone / 64, 0xff2222_cc);
		}
		// Show instructions + title
		int tcy = 8;
		draw::text(matrix.get_inactive_buffer(), "Calibrate ADC", font::tahoma_9::info, 1, tcy, 0x22ff22_cc);
		tcy += 6;
		draw::text(matrix.get_inactive_buffer(), titles[phase], font::lcdpixel_6::info, 1, tcy, 0xff_c);
		tcy += 6;
		if (auto txt = titles2[phase]) {
			draw::text(matrix.get_inactive_buffer(), titles2[phase], font::lcdpixel_6::info, 1, tcy, 0xff_c);
			tcy += 6;
		}
		draw::text(matrix.get_inactive_buffer(), phase == VISUALIZE ? "[PWR] to save" : "[PWR] to continue", font::lcdpixel_6::info, 0, tcy, 0xff_c);
		tcy += 6;

		switch (phase) {
			case CAPTURE_MIN_MAX:
				draw::text(matrix.get_inactive_buffer(), "[MENU] to reset", font::lcdpixel_6::info, 0, tcy, 0xcc_c);
				if (ui::buttons[ui::buttons.MENU]) {
					new_calibration.x.min = new_calibration.x.max = x;
					new_calibration.y.min = new_calibration.y.max = y;
				}
				else {
					new_calibration.x.min = std::min(new_calibration.x.min, x);
					new_calibration.y.min = std::min(new_calibration.y.min, y);
					new_calibration.x.max = std::max(new_calibration.x.max, x);
					new_calibration.y.max = std::max(new_calibration.y.max, y);
				}
				break;
			case CAPTURE_DOWN:
				{
					int dy1 = intmath::abs((int)new_calibration.y.min - (int)y);
					int dy2 = intmath::abs((int)new_calibration.y.max - (int)y);

					if (dy2 > dy1)
						std::swap(new_calibration.y.min, new_calibration.y.max);
				}
				break;
			case CAPTURE_LEFT:
				{
					int dx1 = intmath::abs((int)new_calibration.x.min - (int)x);
					int dx2 = intmath::abs((int)new_calibration.x.max - (int)x);

					if (dx2 < dx1)
						std::swap(new_calibration.x.min, new_calibration.x.max);
				}
				break;
			case MOVE_CENTER:
				break;
			case CAPTURE_DEADZONE:
				draw::text(matrix.get_inactive_buffer(), "[MENU] to reset", font::lcdpixel_6::info, 0, tcy, 0xcc_c);
				if (ui::buttons[ui::buttons.MENU]) {
					new_calibration.x.deadzone = 0;
					new_calibration.y.deadzone = 0;
				}
				else {
					new_calibration.x.deadzone = std::max<uint16_t>(
						intmath::abs(((int)new_calibration.x.min + (int)new_calibration.x.max) / 2 - x), new_calibration.x.deadzone
					);
					new_calibration.y.deadzone = std::max<uint16_t>(
						intmath::abs(((int)new_calibration.y.min + (int)new_calibration.y.max) / 2 - y), new_calibration.y.deadzone
					);
				}
				break;
			case VISUALIZE:
				draw::text(matrix.get_inactive_buffer(), "[MENU] to restart", font::lcdpixel_6::info, 0, tcy, 0xcc_c);
				if (ui::buttons[ui::buttons.MENU]) {
					new_calibration = {
						.x = {
							.min = x,
							.max = x,
							.deadzone = 0,
						},
						.y = {
							.min = y,
							.max = y,
							.deadzone = 0
						}
					};
					phase = CAPTURE_MIN_MAX;
				}
				break;
		}

		if (ui::buttons[ui::buttons.POWER]) {
			if (phase == VISUALIZE)
			{
				draw::rect(matrix.get_inactive_buffer(), 0, 29, 128, 29 + 6, 0x22aa22_cc);
				draw::outline_text(matrix.get_inactive_buffer(), "saving", font::lcdpixel_6::info, 64 - 12, 29 + 5, 0xff_c);
				matrix.swap_buffers();
				if (!servicer.set_adc_calibration(new_calibration)) {
					crash::panic("failed to save calibration");
				}
				break;
			}
			else phase = (Phase)(phase + 1);
		}

		{
			char buf[32];
			snprintf(buf, 32, "x %04d y %04d", x, y);
			draw::text(matrix.get_inactive_buffer(), buf, font::lcdpixel_6::info, 1, 64 - 6, 0x77_c);
		}

		matrix.swap_buffers();
	}

	ui::buttons.reload_adc_calibration();
	last_swapped_at = timekeeper.current_time;
	interact_mode = InteractNone;
}
