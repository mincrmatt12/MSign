#include "grab.h"

// begin grabbers
#include "sccfg.h"
#include "ttc.h"
#include "weather.h"
#include "modelserve.h"
#include "tdsb.h"
// end grabbers

#include "../wifitime.h"
#include <cstring>
#include <algorithm>
#include "../sd.h"
#include "esp_log.h"

namespace grabber {
	constexpr const Grabber * const grabbers[] = {
		&ttc::ttc_grabber,
		&weather::weather_grabber,
		&sccfg::sccfg_grabber,
		&modelserve::modelserve_grabber,
		&tdsb::tdsb_grabber,
	};

	constexpr size_t grabber_count = sizeof(grabbers) / sizeof(grabbers[0]);

	constexpr TickType_t get_max_delay() {
		TickType_t md = 0;
		for (size_t i = 0; i < grabber_count; ++i) {
			md = std::max(md, std::max(grabbers[i]->fail_time, grabbers[i]->loop_time));
		}
		return md + 5000;
	}

	constexpr TickType_t max_delay_between_runs = get_max_delay();

	TickType_t wants_to_run_at[grabber_count]{};

	void run_grabber(size_t i, const Grabber * const grabber) {
		auto ticks = xTaskGetTickCount();
		if (ticks < wants_to_run_at[i]) return;
		wants_to_run_at[i] = xTaskGetTickCount() + (grabber->grab_func() ? grabber->loop_time : grabber->fail_time);
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
			if (target < now || target - now > max_delay_between_runs) {
				ESP_LOGW("grab", "got out of sync in grabber sched (wanted to delay from %u to %u)", now, target);
				if (!(target < now)) {
					vTaskDelay(max_delay_between_runs);
				}
			}
			else vTaskDelay(target - now);

			// Right before re-grabbing, flush logs; this should happen at a fairly unbusy time usually.
			sd::flush_logs();
		}
	}
};
