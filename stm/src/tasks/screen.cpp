#include "screen.h"
#include "../draw.h"
#include "../fonts/dejavu_10.h"
#include "../fonts/lcdpixel_6.h"
#include "../common/bootcmd.h"
#include "../srv.h"

extern srv::Servicer servicer;
extern matrix_type matrix;
extern uint64_t rtc_time;

#ifndef MSIGN_GIT_REV
#define MSIGN_GIT_REV "-unk"
#endif

namespace tasks {
	namespace {
		template <typename FB>
		void show_test_pattern(uint8_t stage, FB& fb, const char * extra=nullptr) {
			fb.clear();
			draw::text(fb, "MSIGN V4.1" MSIGN_GIT_REV, font::lcdpixel_6::info, 0, 7, 0, 4095, 0);
			draw::text(fb, "STM OK", font::lcdpixel_6::info, 0, 21, 4095, 4095, 4095);
			char buf[5] = {0};
			strncpy(buf, bootcmd_get_bl_revision(), 4);
			draw::multi_text(fb, font::lcdpixel_6::info, 0, 14, "BLOAD ", 14, 4095, 127_c, buf, 4095, 4095, 4095);
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

		void show_overlays() {
			slots::WebuiStatus status;
			{
				srv::ServicerLockGuard g(servicer);
				auto& blk = servicer.slot<slots::WebuiStatus>(slots::WEBUI_STATUS);
				if (blk) {
					status = *blk;
				}
				else return;
			}

			int x = 128;
			
			// try to draw sysupgrade
			if (status.flags & slots::WebuiStatus::RECEIVING_SYSUPDATE) {
				// compute size
				int width = draw::text_size("\xfe sys", font::dejavusans_10::info) + 2;

				// blank
				draw::rect(matrix.get_inactive_buffer(), x - width, 0, x, 12, 0, 0, 0);
				x -= width;

				// draw
				draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, x - 1, 9, "\xfe ", 10_c, 245_c, 30_c, "sys", 128_c, 128_c, 128_c);
			}
			
			// try to draw webui
			if (status.flags & slots::WebuiStatus::RECEIVING_WEBUI_PACK) {
				// compute size
				int width = draw::text_size("\xfe ui", font::dejavusans_10::info) + 2;

				// blank
				draw::rect(matrix.get_inactive_buffer(), x - width, 0, x, 12, 0, 0, 0);
				x -= width;

				// draw
				draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, x - 1, 9, "\xfe ", 10_c, 245_c, 30_c, "ui", 128_c, 128_c, 128_c);
			}

			// try to draw failed
			if (status.flags & slots::WebuiStatus::LAST_RX_FAILED) {
				// compute size
				int width = draw::text_size("\xfe err", font::dejavusans_10::info) + 2;

				// blank
				draw::rect(matrix.get_inactive_buffer(), x - width, 0, x, 12, 0, 0, 0);
				x -= width;

				// draw
				draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, x - 1, 9, "\xfe ", 255_c, 5_c, 5_c, "err", 255_c, 255_c, 255_c);
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
		servicer.set_temperature_all(bheap::Block::TemperatureHot, slots::SCCFG_INFO, slots::SCCFG_TIMING, slots::WEBUI_STATUS);

		swapper.notify_before_transition(0, true);
		swapper.transition(0);

		while (1) {
			// Draw active screen
			if (swapper.require_clearing()) matrix.get_inactive_buffer().clear();
			swapper.draw();

			// Check for screen swaps 
			if (servicer[slots::SCCFG_INFO] && servicer[slots::SCCFG_TIMING]) {
				srv::ServicerLockGuard g(servicer);
				if (rtc_time - last_swapped_at > 200'0000 /* if time jumps by more than 200 seconds */ || rtc_time < last_swapped_at) 
					last_swapped_at = rtc_time;

				// Check if the time elapsed is more than the current selection's millis_enabled
				if ((rtc_time - last_swapped_at) > servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[screen_list_idx].millis_enabled) {
					screen_list_idx = next_screen_idx();
					int about_to_show = servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[screen_list_idx].screen_id;
					int next_to_show = servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[next_screen_idx()].screen_id;
					servicer.give_lock();
					swapper.notify_before_transition(about_to_show, true);
					swapper.transition(about_to_show);
					swapper.notify_before_transition(next_to_show, false);
					servicer.take_lock();
					last_swapped_at = rtc_time;
				}
			}

			// show overlays
			show_overlays();

			// Sync on swap buffer
			matrix.swap_buffers();
		}
	}
	
	int DispMan::next_screen_idx() {
		int retval = screen_list_idx;
		do {
			++retval;
			retval %= (servicer[slots::SCCFG_TIMING].datasize / sizeof(slots::ScCfgTime));
			if (retval == screen_list_idx) return retval;
		} while (
			!(servicer.slot<slots::ScCfgInfo>(slots::SCCFG_INFO)->enabled_mask & (1 << 
			  servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[retval].screen_id)
			)
		);
		return retval;
	}
}


