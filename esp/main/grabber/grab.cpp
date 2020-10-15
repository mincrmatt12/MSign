#include "grab.h"

// begin grabbers
#include "sccfg.h"
#include "ttc.h"
#include "weather.h"
#include "modelserve.h"
// end grabbers

#include "../wifitime.h"
#include <cstring>
#include <algorithm>

namespace grabber {
	constexpr const Grabber * const grabbers[] = {
		&ttc::ttc_grabber,
		&weather::weather_grabber,
		&sccfg::sccfg_grabber,
		&modelserve::modelserve_grabber
	};

	constexpr size_t grabber_count = sizeof(grabbers) / sizeof(grabbers[0]);

	TickType_t wants_to_run_at[grabber_count]{};

	void run_grabber(size_t i, const Grabber * const grabber) {
		auto ticks = xTaskGetTickCount();
		if (ticks < wants_to_run_at[i]) return;
		wants_to_run_at[i] = ticks + (grabber->grab_func() ? grabber->loop_time : grabber->fail_time);
	}

	void run(void*) {
		// Wait for wifi
		xEventGroupWaitBits(wifi::events, wifi::WifiConnected, false, true, portMAX_DELAY);
		memset(wants_to_run_at, 0, sizeof(wants_to_run_at));

		// Init all grabbers
		for (size_t i = 0; i < grabber_count; ++i) grabbers[i]->init_func();

		while (true) {
			xEventGroupWaitBits(wifi::events, wifi::WifiConnected, false, true, portMAX_DELAY); // Ensure wifi again (might disconnect at some point)

			// Run all non-ssl grabbers
			for (size_t i = 0; i < grabber_count; ++i) {
				if (!grabbers[i]->ssl) run_grabber(i, grabbers[i]);
			}

			// Wait for time
			xEventGroupWaitBits(wifi::events, wifi::TimeSynced, false, true, portMAX_DELAY);

			// Run all ssl grabbers
			for (size_t i = 0; i < grabber_count; ++i) {
				if (grabbers[i]->ssl) run_grabber(i, grabbers[i]);
			}

			// Wait for minimum element
			auto target = *std::min_element(wants_to_run_at, wants_to_run_at + grabber_count);
			auto now = xTaskGetTickCount();
			if (target < now) continue;
			vTaskDelay(target - now);
		}
	}
};
