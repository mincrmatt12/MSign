#include "screen.h"
#include "../draw.h"
#include "../fonts/dejavu_10.h"
#include "../fonts/lcdpixel_6.h"
#include "../common/bootcmd.h"
#include "../common/ver.h"
#include "../srv.h"
#include "../ui.h"
#include "timekeeper.h"

#ifdef USE_F2
#include <stm32f2xx.h>
#endif

#ifdef USE_F4
#include <stm32f4xx.h>
#endif

extern srv::Servicer servicer;
extern matrix_type matrix;
extern tasks::Timekeeper timekeeper;

#ifndef MSIGN_GIT_REV
#define MSIGN_GIT_REV "-unk"
#endif

namespace tasks {
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


	void show_test_pattern(uint8_t stage, matrix_type::framebuffer_type& fb, const char * extra=nullptr) {
		fb.clear();
		draw::text(fb, "MSIGN V" MSIGN_MAJOR_VERSION_STRING MSIGN_GIT_REV, font::lcdpixel_6::info, 0, 7, 0x00ff00_cc);
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
			case 4:
				draw::text(fb, "ESP CRASH", font::lcdpixel_6::info, 0, 28, 0xff4444_cc);
				break;
			default:
				break;
		}
	}

	void show_overlays(TickType_t& last_had_wifi_at) {
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
		if (!connected && xTaskGetTickCount() - last_had_wifi_at > pdMS_TO_TICKS(1800)) {
			draw::rect(matrix.get_inactive_buffer(), x - 14, 0, x, 12, 0);

			x -= 14;
			// icon
			draw::bitmap(matrix.get_inactive_buffer(), nowifi, 12, 12, 2, x, 0, 0xbb_c);
			draw::line(matrix.get_inactive_buffer(), x, 0, x + 11, 11, 0xff3333_cc);
		}
		else if (connected) {
			last_had_wifi_at = xTaskGetTickCount();
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

		// try to draw cert
		if (status.flags & slots::WebuiStatus::RECEIVING_CERT_PACK) {
			// compute size
			int width = draw::text_size("\xfe cert", font::dejavusans_10::info) + 2;

			// blank
			draw::rect(matrix.get_inactive_buffer(), x - width, 0, x, 12, 0);
			x -= width;

			// draw
			draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, x, 9, "\xfe ", 0x0af31e_cc, "cert", 0x7f7f7f_cc);
		}

		// try to draw webui installing
		if (status.flags & slots::WebuiStatus::INSTALLING_WEBUI_PACK) {
			// compute size
			int width = draw::text_size("\xfd ui", font::dejavusans_10::info) + 2;

			// blank
			draw::rect(matrix.get_inactive_buffer(), x - width, 0, x, 12, 0);
			x -= width;

			// draw
			draw::multi_text(matrix.get_inactive_buffer(), font::dejavusans_10::info, x, 9, "\xfd ", 0x0a1ef3_cc, "ui", 0x7f7f7f_cc);
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

	void DispMan::run() {
		// Short-circuit for sleep mode
		if (bootcmd_get_silent()) {
			matrix.get_inactive_buffer().clear();
			matrix.swap_buffers();
			matrix.get_inactive_buffer().clear();
			matrix.swap_buffers();

			while (!servicer.ready()) {
				matrix.swap_buffers();
				if (servicer.crashed())
					goto when_crashed;
			}

			if (servicer.updating())
				goto do_update;
		}
		else {
			show_test_pattern(0, matrix.get_inactive_buffer());

			matrix.swap_buffers();

			// Init esp comms
			while (!servicer.ready()) {
when_crashed:
				show_test_pattern(servicer.crashed() ? 4 : 1, matrix.get_inactive_buffer());
				matrix.swap_buffers();
			}

			if (servicer.updating()) {
do_update:
				while (true) {
					show_test_pattern(3, matrix.get_inactive_buffer(), servicer.update_status());
					matrix.swap_buffers();
					update_cookie = servicer.update_status_version();
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
		}

		ui::buttons.init();

		// Tell servicer to grab the sccfg
		servicer.set_temperature_all(bheap::Block::TemperatureHot, slots::SCCFG_INFO, slots::SCCFG_TIMING, slots::WEBUI_STATUS, slots::WIFI_STATUS);

		swapper.notify_before_transition(0, true);
		swapper.transition(0);

		if (bootcmd_get_silent()) {
			do_sleep_mode();
		}

		while (1) {
			ui::buttons.update();
			// If we went into sleep mode at some point, enter that handler
			if (servicer.is_sleeping()) do_sleep_mode();
			// If tetris mode is scheduled, go to its handler
			if (interact_mode == InteractTetris) do_tetris_mode();
			else if (interact_mode == InteractAdcCalibration || ui::buttons.adc_state() == ui::buttons.ADC_UNCALIBRATED) do_adc_calibration();

			// Draw active screen
			if (swapper.require_clearing()) matrix.get_inactive_buffer().clear();
			swapper.draw();

			// Check for screen swaps 
			if (servicer[slots::SCCFG_INFO] && servicer[slots::SCCFG_TIMING] && !interacting(false) && override_timeout < xTaskGetTickCount()) {
				srv::ServicerLockGuard g(servicer);
				if (servicer.slot_dirty(slots::SCCFG_TIMING)) {
					if (screen_list_idx >= (servicer[slots::SCCFG_TIMING].datasize / sizeof(slots::ScCfgTime))) screen_list_idx = 0; // handle changing array
				}

				if (timekeeper.current_time - last_swapped_at > 200'0000 /* if time jumps by more than 200 seconds */ || timekeeper.current_time < last_swapped_at) 
					last_swapped_at = timekeeper.current_time;

				// Check if the time elapsed is more than the current selection's millis_enabled
				if ((timekeeper.current_time - last_swapped_at) > servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[screen_list_idx].millis_enabled ||
						ui::buttons[ui::Buttons::NXT]) {
					screen_list_idx = next_screen_idx();
					int about_to_show = servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[screen_list_idx].screen_id;
					int next_to_show = servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[next_screen_idx()].screen_id;
					servicer.give_lock();
					swapper.notify_before_transition(about_to_show, true);
					swapper.transition(about_to_show);
					swapper.notify_before_transition(next_to_show, false);
					servicer.take_lock();
					last_swapped_at = timekeeper.current_time;
					early_transition_informed = false;
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
					last_swapped_at = timekeeper.current_time;
					early_transition_informed = false;
				}

				// Let screen perform more early initialization at least 1.5 seconds before it's shown.
				if ((timekeeper.current_time - last_swapped_at + 1500) > servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[screen_list_idx].millis_enabled && !early_transition_informed) {
					early_transition_informed = true;
					int about_to_show = servicer.slot<slots::ScCfgTime *>(slots::SCCFG_TIMING)[next_screen_idx()].screen_id;
					servicer.give_lock();
					swapper.notify_before_transition(about_to_show, true);
					servicer.take_lock();
				}
			}

			if (interact_mode) {
				if (xTaskGetTickCount() > interact_timeout || 
					ui::buttons.held(ui::Buttons::POWER, pdMS_TO_TICKS(1500))) {
					interact_mode = InteractNone;
					last_swapped_at = timekeeper.current_time;
				}
				if (ui::buttons.changed()) interact_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
			}

			// Everything but menu open
			switch (interact_mode) {
				case InteractNone:
					// allow opening modes
					if (ui::buttons[ui::Buttons::SEL]) {
						interact_mode = InteractByScreen;
						interact_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
					}
					else if (ui::buttons[ui::Buttons::MENU]) {
						interact_mode = InteractMenuOpen;
						interact_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
						ms.reset();
					}
					else if (ui::buttons[ui::Buttons::POWER] && override_timeout > xTaskGetTickCount()) {
						override_timeout = xTaskGetTickCount();
						last_swapped_at = timekeeper.current_time;
					}
					else if (ui::buttons[ui::Buttons::POWER] && op) {
						close_panel();
					}
					else if (ui::buttons.held(ui::Buttons::POWER, pdMS_TO_TICKS(2000))) do_sleep_mode();
					break;
				case InteractByScreen:
					draw::rect(matrix.get_inactive_buffer(), 126, 62, 128, 64, 0x00ff00_cc);
					if (swapper.interact()) {
						// reset swapper timer to avoid the screen instantly going away.
						last_swapped_at = timekeeper.current_time;
						interact_mode = InteractNone;
					}
					break;
				default:
					break;
			}

			// Show panel overlays
			do_overlay_panels();

			if (interact_mode == InteractMenuOpen) do_menu_overlay(); // draw on top of overlay panels...

			// ... but behind icons
			show_overlays(last_had_wifi_at);

			// Sync on swap buffer
			matrix.swap_buffers();
		}
	}

	void DispMan::do_sleep_mode() {
		if (!servicer.is_sleeping()) servicer.set_sleep_mode(true);

		// Wait 10 frames for fade
		for (int i = 0; i < 10; ++i) {
			// Draw active screen
			if (swapper.require_clearing()) matrix.get_inactive_buffer().clear();
			swapper.draw();
			// Fade out
			for (int j = 0; j < 128; ++j) {
				for (int k = 0; k < 64; ++k) {
					matrix.get_inactive_buffer().at_unsafe(j, k).set_spare(0);
					matrix.get_inactive_buffer().at_unsafe(j, k) = matrix.get_inactive_buffer().at_unsafe(j, k).mix(0, i * 255 / 10);
				}
			}
			// swap buffers
			matrix.swap_buffers();
		}

		bool ig = true;
		matrix.force_off = true;

		// Spin
		while (servicer.is_sleeping()) {
			matrix.get_inactive_buffer().clear();
			matrix.swap_buffers();
			// check for power button
			ui::buttons.update();
			if (ui::buttons.held(ui::Buttons::POWER, pdMS_TO_TICKS(1000))) {
				if (ig == true) continue;
				break;
			}
			else ig = false;
		}
		matrix.force_off = false;
		if (servicer.is_sleeping()) servicer.set_sleep_mode(false);
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

	void DispMan::do_tetris_mode() {
		// start tetris screen
		auto prev = swapper.transition(5);
		interact_mode = InteractTetris;

		while (interact_mode != InteractNone) {
			matrix.get_inactive_buffer().clear();
			swapper.draw();
			ui::buttons.update();

			switch (interact_mode) {
				case InteractTetris:
					if (ui::buttons[ui::Buttons::MENU]) {
						ms.selected = 0;
						ms.submenu = MS::SubmenuTetris;
						interact_mode = InteractTetrisMenu;
						swapper.get<screen::game::Tetris>().pause();
					}
					break;
				case InteractTetrisMenu:
					do_menu_overlay();
					break;
				default:
					break;
			}

			show_overlays(last_had_wifi_at);

			matrix.swap_buffers();
		}

		while (ui::buttons.held(ui::Buttons::SEL)) {
			ui::buttons.update(); // wait for button to be unpressed
			matrix.swap_buffers();
		}

		swapper.transition(prev);
		last_swapped_at = timekeeper.current_time;

		interact_mode = InteractNone;
	}
}


