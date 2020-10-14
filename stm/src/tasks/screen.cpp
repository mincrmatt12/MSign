#include "screen.h"
#include "../draw.h"
#include "../fonts/dejavu_10.h"
#include "../fonts/lcdpixel_6.h"
#include "../common/bootcmd.h"
#include "../srv.h"

extern srv::Servicer servicer;
extern matrix_type matrix;

#ifndef MSIGN_GIT_REV
#define MSIGN_GIT_REV "-unk"
#endif

namespace tasks {
	namespace {
		template <typename FB>
		void show_test_pattern(uint8_t stage, FB& fb, const char * extra=nullptr) {
			fb.clear();
			draw::text(fb, "MSIGN V4.0" MSIGN_GIT_REV, font::lcdpixel_6::info, 0, 7, 0, 4095, 0);
			draw::text(fb, "STM OK", font::lcdpixel_6::info, 0, 21, 4095, 4095, 4095);
			char buf[5] = {0};
			strncpy(buf, bootcmd_get_bl_revision(), 4);
			draw::text(fb, buf, font::lcdpixel_6::info, draw::text(fb, "BLOAD ", font::lcdpixel_6::info, 0, 14, 4095, 127_c, 0), 14, 4095, 4095, 4095);
			switch (stage) {
				case 1:
					draw::text(fb, "ESP WAIT", font::lcdpixel_6::info, 0, 28, 4095, 0, 0);
					break;
				case 2:
					draw::text(fb, "ESP OK", font::lcdpixel_6::info, 0, 28, 4095, 4095, 4095);
					draw::text(fb, "UPD NONE", font::lcdpixel_6::info, 0, 35, 0, 4095, 0);
					break;
				case 3:
					draw::text(fb, "ESP OK", font::lcdpixel_6::info, 0, 28, 4095, 4095, 4095);
					if (extra) {
						draw::text(fb, extra, font::lcdpixel_6::info, 0, 35, 40_c, 40_c, 4095);
					}
					else {
						draw::text(fb, "UPD INIT", font::lcdpixel_6::info, 0, 35, 40_c, 40_c, 4095);
					}
					break;
				default:
					break;
			}
		}
	}

	void DispMan::run() {
		show_test_pattern(0, matrix.get_inactive_buffer());

		matrix.swap_buffers();

		// Init esp comms
		while (!servicer.ready()) {
			show_test_pattern(1, matrix.get_inactive_buffer());
			matrix.swap_buffers();
		}

		if (servicer.updating()) {
			// Go into a simple servicer only update mode
			while (true) {
				show_test_pattern(3, matrix.get_inactive_buffer(), servicer.update_status());
				matrix.swap_buffers();
			}

			// Servicer will eventually reset the STM.
		}

		show_test_pattern(2, matrix.get_inactive_buffer());
		matrix.swap_buffers();
		show_test_pattern(2, matrix.get_inactive_buffer());
		matrix.swap_buffers();

		uint16_t finalize_counter = 60;
		while (finalize_counter--) {
			if (finalize_counter >= 50) matrix.swap_buffers();
			else if (finalize_counter >= 34) {
				draw::fill(matrix.get_inactive_buffer(), 4095, 0, 0);
				matrix.swap_buffers();
			}
			else if (finalize_counter >= 18) {
				draw::fill(matrix.get_inactive_buffer(), 0, 4095, 0);
				matrix.swap_buffers();
			}
			else {
				draw::fill(matrix.get_inactive_buffer(), 0, 0, 4095);
				matrix.swap_buffers();
			}
		}

		// Tell servicer to grab the sccfg
		servicer.set_temperature_all(bheap::Block::TemperatureHot, slots::SCCFG_INFO, slots::SCCFG_TIMING);

		// set active screen: TODO port back the auto switcher

		swapper.notify_before_transition(0, true);
		swapper.transition(0);

		while (1) {
			// Draw active screen
			if (swapper.require_clearing()) matrix.get_inactive_buffer().clear();
			swapper.draw();

			// Check for screen swaps ... todo ...

			// Sync on swap buffer
			matrix.swap_buffers();
		}
	}

}
