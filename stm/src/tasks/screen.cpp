#include "screen.h"
#include "../draw.h"
#include "../fonts/dejavu_10.h"
#include "../fonts/lcdpixel_6.h"
#include "../common/bootcmd.h"
#include "../srv.h"
#include "../ui.h"

extern srv::Servicer servicer;
extern matrix_type matrix;
extern uint64_t rtc_time;

#ifndef MSIGN_GIT_REV
#define MSIGN_GIT_REV "-unk"
#endif

namespace tasks {
	namespace {
		// w=12, h=12, stride=2, color=255, 255, 255
		const uint8_t nowifi[] = {
			0b00000000,0b00000000,
			0b00000000,0b00000000,
			0b00001111,0b00000000,
			0b00110000,0b11000000,
			0b01000000,0b00100000,
			0b10011111,0b10010000,
			0b00100000,0b01000000,
			0b01001111,0b00100000,
			0b00010000,0b10000000,
			0b00100000,0b01000000,
			0b00000110,0b00000000,
			0b00000110,0b00000000
		};


		template <typename FB>
		void show_test_pattern(uint8_t stage, FB& fb, const char * extra=nullptr) {
			fb.clear();
			draw::text(fb, "MSIGN V4.1" MSIGN_GIT_REV, font::lcdpixel_6::info, 0, 7, 0x00ff00_cc);
			draw::text(fb, "STM OK", font::lcdpixel_6::info, 0, 21, {4095});
			char buf[5] = {0};
			strncpy(buf, bootcmd_get_bl_revision(), 4);
			draw::multi_text(fb, font::lcdpixel_6::info, 0, 14, "BLOAD ", led::color_t{14, 4095, 127_c}, buf, led::color_t{4095});
			switch (stage) {
				case 1:
					draw::text(fb, "ESP WAIT", font::lcdpixel_6::info, 0, 28, 0xff0000_cc);
					break;
				case 2:
					draw::text(fb, "ESP OK", font::lcdpixel_6::info, 0, 28, 0xffffff_cc);
					draw::text(fb, "UPD NONE", font::lcdpixel_6::info, 0, 35, 0x00ff00_cc);
					break;
				case 3:
					draw::text(fb, "ESP OK", font::lcdpixel_6::info, 0, 28, 0xffffff_cc);
					if (extra) {
						draw::text(fb, extra, font::lcdpixel_6::info, 0, 35, 0x2828ff_cc);
					}
					else {
						draw::text(fb, "UPD INIT", font::lcdpixel_6::info, 0, 35, 0x2828ff_cc);
					}
					break;
				default:
					break;
			}
		}

		void show_overlays() {
			slots::WebuiStatus status{};
			bool connected = true;

			{
				srv::ServicerLockGuard g(servicer);
				auto& blk = servicer.slot<slots::WebuiStatus>(slots::WEBUI_STATUS);
				if (blk) {
					status = *blk;
				}
				if (auto &wf = servicer.slot<slots::WifiStatus>(slots::WIFI_STATUS); wf && !wf->connected) connected = false;
			}

			int x = 128;

			// show wifi fail
			if (!connected) {
				draw::rect(matrix.get_inactive_buffer(), x - 14, 0, x, 12, 0);

				x -= 14;
				// icon
				draw::bitmap(matrix.get_inactive_buffer(), nowifi, 12, 12, 2, x, 0, 0xbb_c);
				draw::line(matrix.get_inactive_buffer(), x, 0, x + 11, 11, 0xff3333_cc);
			}
			
			// try to draw sysupgrade
			if (status.flags & slots::WebuiStatus::RECEIVING_SYSUPDATE) {
				// compute size
				int width = draw::text_size("\xfe sys", font::dejavusans_10::info) + 2;

				// blank
				draw::rect(matrix.get_inactive_buffer(), x - width, 0, x, 12, 0);
				x -= width;

				// draw
				draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, x, 9, "\xfe ",led::color_t {10_c, 245_c, 30_c}, "sys", led::color_t{128_c, 128_c, 128_c});
			}
			
			// try to draw webui
			if (status.flags & slots::WebuiStatus::RECEIVING_WEBUI_PACK) {
				// compute size
				int width = draw::text_size("\xfe ui", font::dejavusans_10::info) + 2;

				// blank
				draw::rect(matrix.get_inactive_buffer(), x - width, 0, x, 12, 0);
				x -= width;

				// draw
				draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, x, 9, "\xfe ", 0x0af31e_cc, "ui", 0x7f7f7f_cc);
			}

			// try to draw failed
			if (status.flags & slots::WebuiStatus::LAST_RX_FAILED) {
				// compute size
				int width = draw::text_size("\xfe err", font::dejavusans_10::info) + 2;

				// blank
				draw::rect(matrix.get_inactive_buffer(), x - width, 0, x, 12, 0);
				x -= width;

				// draw
				draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, x, 9, "\xfe ", 0xff0505_cc, "err", 0xffffff_cc);
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

		ui::buttons.init();

		show_test_pattern(2, matrix.get_inactive_buffer());
		matrix.swap_buffers();
		show_test_pattern(2, matrix.get_inactive_buffer());
		matrix.swap_buffers();

		uint16_t finalize_counter = 60;
		while (finalize_counter--) {
			if (finalize_counter >= 50) matrix.swap_buffers();
			else if (finalize_counter >= 34) {
				draw::gradient_rect(matrix.get_inactive_buffer(), 0, 0, 128, 64, 0, 0xff0000_ccu);
				matrix.swap_buffers();
			}
			else if (finalize_counter >= 18) {
				draw::gradient_rect(matrix.get_inactive_buffer(), 0, 0, 128, 64, 0, 0x00ff00_ccu);
				matrix.swap_buffers();
			}
			else {
				draw::gradient_rect(matrix.get_inactive_buffer(), 0, 0, 128, 64, 0, 0x0000ff_ccu);
				matrix.swap_buffers();
			}
		}

		// Tell servicer to grab the sccfg
		servicer.set_temperature_all(bheap::Block::TemperatureHot, slots::SCCFG_INFO, slots::SCCFG_TIMING, slots::WEBUI_STATUS, slots::WIFI_STATUS);

		swapper.notify_before_transition(0, true);
		swapper.transition(0);

		while (1) {
			ui::buttons.update();
			// Draw active screen
			if (swapper.require_clearing()) matrix.get_inactive_buffer().clear();
			swapper.draw();

			// Check for screen swaps 
			if (servicer[slots::SCCFG_INFO] && servicer[slots::SCCFG_TIMING] && !interacting()) {
				srv::ServicerLockGuard g(servicer);
				if (rtc_time - last_swapped_at > 200'0000 /* if time jumps by more than 200 seconds */ || rtc_time < last_swapped_at) 
					last_swapped_at = rtc_time;

				// Check if the time elapsed is more than the current selection's millis_enabled
				if ((rtc_time - last_swapped_at) > servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[screen_list_idx].millis_enabled ||
						ui::buttons[ui::Buttons::NXT]) {
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
				else if (ui::buttons[ui::Buttons::PRV]) {
					screen_list_idx = next_screen_idx(true);
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

			if (interact_mode) {
				if (xTaskGetTickCount() > interact_timeout || 
					ui::buttons.held(ui::Buttons::POWER, pdMS_TO_TICKS(1500))) {
					interact_mode = InteractNone;
					last_swapped_at = rtc_time;
				}
				if (ui::buttons.changed()) interact_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
			}

			switch (interact_mode) {
				case InteractNone:
					// allow opening modes
					if (ui::buttons[ui::Buttons::SEL]) {
						interact_mode = InteractByScreen;
						interact_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
					}
					/*else if (ui::buttons[ui::Buttons::MENU]) {
						interact_mode = InteractMenuOpen;
						interact_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
					}*/
					break;
				case InteractByScreen:
					// TODO: draw overlay?
					draw::rect(matrix.get_inactive_buffer(), 126, 62, 128, 64, 0x00ff00_cc);
					if (swapper.interact()) {
						// reset swapper timer to avoid the screen instantly going away.
						last_swapped_at = rtc_time;
						interact_mode = InteractNone;
					}
					break;
				default:
					break;
			}

			// show overlays
			show_overlays();

			// Sync on swap buffer
			matrix.swap_buffers();
		}
	}
	
	int DispMan::next_screen_idx(bool prev) {
		uint8_t retval = screen_list_idx;
		do {
			if (!prev) {
				++retval;
				retval %= (servicer[slots::SCCFG_TIMING].datasize / sizeof(slots::ScCfgTime));
			}
			else {
				--retval;
				retval = std::min<uint8_t>(retval, servicer[slots::SCCFG_TIMING].datasize / sizeof(slots::ScCfgTime)-1);
			}
			if (retval == screen_list_idx) return retval;
		} while (
			!(servicer.slot<slots::ScCfgInfo>(slots::SCCFG_INFO)->enabled_mask & (1 << 
			  servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[retval].screen_id)
			)
		);
		return retval;
	}
}


