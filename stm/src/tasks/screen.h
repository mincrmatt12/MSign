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

		int screen_list_idx = 0;
		uint64_t last_swapped_at = 0;

		int next_screen_idx();
	};
}

#endif
