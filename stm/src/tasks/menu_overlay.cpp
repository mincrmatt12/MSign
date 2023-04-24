#include "screen.h"
#include "../draw.h"
#include "../srv.h"
#include "../crash/main.h"

#include "../fonts/tahoma_9.h"
#include "timekeeper.h"

extern srv::Servicer servicer;
extern matrix_type matrix;
extern tasks::Timekeeper timekeeper;

namespace {
	struct OverlayPanel {
		OverlayPanel(const OverlayPanel&) = delete;
		OverlayPanel() {};

		void text(const char * text, led::color_t clr=0xff_c) {
			multi_text(text, clr);
		}

		template<typename ...TextThenColors>
		void multi_text(TextThenColors&& ...colors) {
			int width = (lenient_strsize(colors) + ...) + 1;
			draw_bg_with_new_width(width, 9);
			draw::multi_text(matrix.get_inactive_buffer(), font::tahoma_9::info, pos + border + padding + 1, y - 2, std::forward<TextThenColors>(colors)...);
		}

		~OverlayPanel() {
			draw::outline(matrix.get_inactive_buffer(), 1, 1, contentmaxx, contentmaxy, 0xff_c);
		}

	private:
		void draw_bg_with_new_width(int16_t width, int16_t height) {
			int16_t newwidth = std::max<int16_t>(pos + width + padding * 2 + border * 2, contentmaxx);
			// vertical strip
			draw::rect(matrix.get_inactive_buffer(), pos, y, newwidth, y + height, 0);
			// horizontal strip (if required)
			if (newwidth != contentmaxx)
				draw::rect(matrix.get_inactive_buffer(), contentmaxx, pos, newwidth, y, 0);
			contentmaxx = newwidth;
			y += height;
			contentmaxy = std::max(y, contentmaxy);
		}

		template<typename T>
		inline int lenient_strsize(const T& t) {
			if constexpr (std::is_convertible_v<decltype(t), const char *>) {
				return draw::text_size(t, font::tahoma_9::info);
			}
			else return 0;
		}

		int16_t contentmaxx = pos + border;
		int16_t contentmaxy = pos + border;
		int16_t y = pos + border + padding;

		const static inline int padding = 1;
		const static inline int border  = 1;
		const static inline int pos     = 1;
	};

	void draw_conn_panel() {
		srv::ServicerLockGuard g(servicer);

		const auto& conndata = servicer.slot<slots::WifiStatus>(slots::WIFI_STATUS);
		char ipbuf[32];

		OverlayPanel op;

		if (!conndata) {
			op.text("unknown", 0x80_c);
		}
		else {
			if (conndata->connected) {
				op.multi_text("connected: ", 0xff_c, "yes", 0x44ff44_cc);
				snprintf(ipbuf, 32, "ip: %d.%d.%d.%d", conndata->ipaddr[0], conndata->ipaddr[1], conndata->ipaddr[2], conndata->ipaddr[3]);
				op.text(ipbuf);
				snprintf(ipbuf, 32, "gw: %d.%d.%d.%d", conndata->gateway[0], conndata->gateway[1], conndata->gateway[2], conndata->gateway[3]);
				op.text(ipbuf);
			}
			else {
				op.multi_text("connected: ", 0xff_c, "no", 0xff4444_cc);
			}
		}
	}

	// various debug panels
	void draw_debug_srv_panel() {
		OverlayPanel op;
		char buf[32];
		
		srv::Servicer::DebugInfo di = servicer.get_debug_information();

		snprintf(buf, 32, "%d", (int)di.pending_requests_count);
		op.multi_text("pendreq: ", 0xff_c, buf, (di.pending_requests_count < 4 ? 0x44ff44_cc : 0xff6666_cc));
		snprintf(buf, 32, "%05d ms", (int)(di.ticks_since_last_communication * portTICK_PERIOD_MS));
		op.multi_text("time: ", 0xff_c, buf, 0xff_c, "; ping? ", 0xff_c, di.ping_is_in_flight ? "true" : "false", 0x4444ff_cc);
		snprintf(buf, 32, "%d / %d / %d", (int)di.free_space_arena, (int)di.free_space_cleanup_arena, (int)di.used_hot_space_arena);
		op.multi_text("f/c/h: ", 0xff_c, buf, 0xff_c);
	}
}

void tasks::DispMan::draw_menu_list(const char * const * entries, bool last_is_close) {
	// Compute size

	int padding = 2; // 1 pixel both sides
	int border = 2; // 1 pixel both sides

	int leftoffset = 1 + padding / 2 + border / 2;

	int contentwidth = 0;
	int contentheight = 0;
	int caretsize = draw::text_size("> ", font::tahoma_9::info);
	int max = 0;

	for (auto entry = entries; *entry; entry++) {
		contentwidth = std::max(contentwidth, draw::text_size(*entry, font::tahoma_9::info) + caretsize);
		contentheight += 9;
		++max;
	}

	int y = 1;

	// Allow scrolling
	if (padding + contentheight + border > 64) {
		int dheight = (y + padding / 2 + 7 + border / 2 + 9 * ms.selected);
		// Move to middle
		if (dheight > 37) {
			y -= (dheight - 37);
		}
		// Move to bottom
		if ((y + padding + contentheight + border) < 58) {
			y += (58 - (y + padding + contentheight + border));
		}
	}

	// Draw a rectangle onscreen
	draw::rect(matrix.get_inactive_buffer(), 1, y, 1 + padding + contentwidth + border, y + padding + contentheight + border, 0);
	draw::outline(matrix.get_inactive_buffer(), 1, y, 1 + padding + contentwidth + border, y + padding + contentheight + border, 0xff_c);

	// Draw entries
	y += padding / 2 + 7 + border / 2;
	for (int i = 0; i < max; ++i) {
		auto tc = (i == max - 1 && last_is_close) ? 0xcc_c : 0xff_c;
		if (ms.selected == i) {
			draw::multi_text(matrix.get_inactive_buffer(), font::tahoma_9::info, leftoffset, y, "> ", 0x3344fc_cc, entries[i], tc);
		}
		else {
			draw::multi_text(matrix.get_inactive_buffer(), font::tahoma_9::info, leftoffset, y, entries[i], tc);
		}
		y += 9;
	}

	if (ui::buttons[ui::Buttons::NXT]) {
		ms.selected += 1;
		ms.selected %= max;
	}
	else if (ui::buttons[ui::Buttons::PRV]) {
		if (ms.selected == 0) ms.selected = max - 1;
		else --ms.selected;
	}
}

void tasks::DispMan::do_overlay_panels() {
	switch (op) {
		case OverlayPanelDebugSrv:
			draw_debug_srv_panel();
			break;
		case OverlayPanelConnInfo:
			draw_conn_panel();
			break;
		default:
			break;
	}
}

void tasks::DispMan::do_menu_overlay() {
	const static char* const menu_entries[] = {
		"refresh",
		"goto",
		"connection",
		"check for update",
		"close",
		nullptr
	};

	const static char * const screen_names[] = {
		"transit",
		"weather",
		"clock",
		"parcels",

		"back",
		nullptr
	};

	const static char * const debug_entries[] = {
		"reset",
		"crash (panic)",
		"crash (nf panic)",
		"crash (mem)",
		"crash (wdog)",
		"enable div0 trp",
		"servicer debug",
		"T E T R I S",
		nullptr
	};

	const static char * const tetris_entries[] = {
		"back to game",
		"restart game",

		"exit to sign",
		nullptr
	};

	switch (ms.submenu) {
		case MS::SubmenuMain:
			draw_menu_list(menu_entries);

			if (ui::buttons.rel(ui::Buttons::SEL)) {
				if (ms.selected == 0) {
					swapper.refresh();
					goto close;
				}
				else if (ms.selected == 1) {
					ms.selected = 0;
					ms.submenu = MS::SubmenuSelectScreen;
					return;
				}
				else if (ms.selected == 2) {
					ms.selected = 0;
					open_panel(OverlayPanelConnInfo);
					goto close;
				}
				else if (ms.selected == 3) {
					servicer.refresh_grabber(slots::protocol::GrabberID::CFGPULL);
					goto close;
				}
				else if (ms.selected == 4) goto close;
			}
			else if (ui::buttons.held(ui::Buttons::MENU, pdMS_TO_TICKS(1800))) {
				ms.selected = 0;
				ms.submenu = MS::SubmenuDebug;
				return;
			}
			else if (ui::buttons.held(ui::Buttons::SEL, pdMS_TO_TICKS(800)) && ms.selected == 0) {
				servicer.refresh_grabber(slots::protocol::GrabberID::ALL);
				goto close;
			}

			break;
		case MS::SubmenuSelectScreen:
			draw_menu_list(screen_names);

			if (ui::buttons[ui::Buttons::SEL]) {
				if (ms.selected == (sizeof(screen_names)/sizeof(char*) - 2)) goto back;

				swapper.transition(ms.selected);
				override_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(30000);
				last_swapped_at = timekeeper.current_time;
				interact_mode = InteractNone;
				return;
			}
			break;
		case MS::SubmenuDebug:
			draw_menu_list(debug_entries, false);

			if (ui::buttons[ui::Buttons::SEL]) {
				if (ms.selected == 0) {
					servicer.reset();
				}
				else if (ms.selected == 1) {
					crash::panic("test panic");
				}
				else if (ms.selected == 2) {
					crash::panic_nonfatal("test nonfatal");
				}
				else if (ms.selected == 3) {
					volatile uint32_t * the_void = (uint32_t *)(0x20021234);
					(void)(*the_void);
				}
				else if (ms.selected == 4) {
					vTaskDelay(pdMS_TO_TICKS(30'000));
				}
				else if (ms.selected == 5) {
#ifndef SIM
					SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
#endif
					goto close;
				}
				else if (ms.selected == 6) {
					ms.selected = 0;
					open_panel(OverlayPanelDebugSrv);
					goto close;
				}
				else if (ms.selected == 7) {
					ms.selected = 0;
					ms.submenu = MS::SubmenuTetris;
					interact_mode = InteractTetris;
					return;
				}
			}
			break;
		case MS::SubmenuTetris:
			draw_menu_list(tetris_entries);

			if (ui::buttons[ui::Buttons::SEL]) {
				if (ms.selected == 0) goto closetetris;
				else if (ms.selected == 1) {
					swapper.get<screen::game::Tetris>().restart();
					goto closetetris;
				}
				else if (ms.selected == 2) goto close;
			}
			break;
	}

	if (ui::buttons[ui::Buttons::POWER]) {
		switch (ms.submenu) {
			case MS::SubmenuTetris:
closetetris:
				interact_mode = InteractTetris;
				swapper.get<screen::game::Tetris>().unpause();
				break;
			case MS::SubmenuMain:
			case MS::SubmenuDebug:
close:
				last_swapped_at = timekeeper.current_time;
				interact_mode = InteractNone;
				break;
			default:
back:
				ms.submenu = MS::SubmenuMain;
				break;
		}
	}
}
