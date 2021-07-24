#ifndef MSN_SCREEN_H
#define MSN_SCREEN_H

#include "../screens/base.h"

#include "../screens/ttc.h"
#include "../screens/weather.h"
#include "../screens/clock.h"
#include "../screens/threed.h"

namespace tasks {
	struct DispMan {
		void run();

	private:
		screen::ScreenSwapper<
			screen::TTCScreen,
			screen::WeatherScreen,
			screen::LayeredScreen<
				threed::Renderer,
				screen::ClockScreen
			>
		> swapper;

		uint8_t screen_list_idx = 0;
	
		enum InteractMode : uint8_t {
			InteractNone = 0,
			InteractByScreen = 1,
			InteractMenuOpen = 2,
			InteractOverrideScreen = 3
		} interact_mode = InteractNone;

		// todo: put this in a union
		// uint8_t menu_id, menu_selection; -- put this to a separate class thingy

		uint64_t last_swapped_at = 0;
		uint32_t interact_timeout = 0;

		int next_screen_idx(bool prev=false);

		bool interacting() const {
			return interact_timeout && interact_mode;
		}
	};
}

#endif
