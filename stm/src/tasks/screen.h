#ifndef MSN_SCREEN_H
#define MSN_SCREEN_H

#include "../screens/base.h"

#include "../screens/ttc.h"
#include "../screens/weather.h"
#include "../screens/clock.h"
#include "../screens/threed.h"
#include "../screens/parcels.h"

#include "../screens/tetris.h"

namespace tasks {
	struct DispMan {
		void run();

		bool interacting() const {
			return interact_timeout && interact_mode;
		}
	private:
		screen::ScreenSwapper<
			screen::TTCScreen,
			screen::WeatherScreen,
			screen::LayeredScreen<
				threed::Renderer,
				screen::ClockScreen
			>,
			screen::ParcelScreen,

			// hidden screens

			screen::game::Tetris
		> swapper;

		uint8_t screen_list_idx = 0;
	
		enum InteractMode : uint8_t {
			InteractNone = 0,
			InteractByScreen = 1,
			InteractMenuOpen = 2,
			InteractTetris = 3,
			InteractTetrisMenu = 4
		} interact_mode = InteractNone;

		// todo: put this in a union
		// uint8_t menu_id, menu_selection; -- put this to a separate class thingy

		uint64_t last_swapped_at = 0;
		uint32_t interact_timeout = 0;
		uint32_t override_timeout = 0;
		TickType_t last_had_wifi_at = 0;

		bool early_transition_informed = false;
		int next_screen_idx(bool prev=false);

		// menu/interaction stuff -- in overlay.cpp

		struct MS {
			uint8_t selected = 0;
			enum Submenu : uint8_t {
				SubmenuMain = 0,
				//SubmenuConnInfo = 1,
				SubmenuSelectScreen = 2,
				SubmenuDebug = 3,
				// SubmenuDebugSrv = 4,
				SubmenuTetris = 5,
			} submenu = SubmenuMain;

			void reset() {
				new (this) MS{};
			}
		} ms;

		enum OverlayPanel : uint8_t {
			OverlayPanelClosed = 0,
			OverlayPanelDebugSrv = 1,
			OverlayPanelConnInfo = 2
		} op;

		void open_panel(OverlayPanel pan) {
			if (pan == op) op = OverlayPanelClosed; // toggle semantics
			else op = pan;
		}

		void draw_menu_list(const char * const * entries, bool last_is_close=true);

		void do_overlay_panels();
		void do_menu_overlay();
		void do_sleep_mode();
		void do_tetris_mode();
	};
}

#endif
