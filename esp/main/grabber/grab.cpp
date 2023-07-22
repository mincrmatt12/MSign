#include "grab.h"

// begin grabbers
#include "sccfg.h"
#include "transit/ttc.h"
#include "transit/gtfs.h"
#include "weather.h"
#include "modelserve.h"
#include "parcels.h"
#include "cfgpull.h"
// end grabbers

#include "../wifitime.h"
#include <cstring>
#include <algorithm>
#include "../sd.h"
#include "esp_log.h"
#include "../serial.h"
#include "../dwhttp.h"
#include <esp_system.h>

#ifndef SIM
#include "esp_ota_ops.h"
#endif

namespace grabber {
	constexpr const Grabber * const grabbers[] = {
		&transit::ttc::ttc_grabber,
		&transit::gtfs::gtfs_grabber,
		&weather::weather_grabber,
		&sccfg::sccfg_grabber,
		&modelserve::modelserve_grabber,
		&parcels::parcels_grabber,

		// leave cfgpull at the end
		&cfgpull::cfgpull_grabber
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
	slots::protocol::GrabberID refresh_for = slots::protocol::GrabberID::NONE;

	void refresh(slots::protocol::GrabberID gid) {
		if (refresh_for != slots::protocol::GrabberID::NONE) {
			refresh_for = slots::protocol::GrabberID::ALL;
			return;
		}
		refresh_for = gid;
		xEventGroupSetBits(wifi::events, wifi::GrabRequested);
	}

	TickType_t wants_to_run_at[grabber_count]{};
	TaskHandle_t grabber_task = nullptr;

	void run_grabber(size_t i, const Grabber * const grabber) {
		auto ticks = xTaskGetTickCount();
		
		if (
			(refresh_for == grabber->associated && grabber->refreshable) || 
			(refresh_for == slots::protocol::GrabberID::ALL) ||
			(ticks > wants_to_run_at[i])
		) { 
			wants_to_run_at[i] = xTaskGetTickCount() + (grabber->grab_func() ? grabber->loop_time : grabber->fail_time) * (serial::interface.is_sleeping() && i != grabber_count-1 ? 5 : 1);
		}
	}

	void reload_all() {
		// Init all grabbers
		for (size_t i = 0; i < grabber_count; ++i) grabbers[i]->init_func();
	}

	void run(void*) {
#define stoppable(x) if (xEventGroupWaitBits(wifi::events, (x) | wifi::GrabTaskStop, false, false, portMAX_DELAY) & wifi::GrabTaskStop) { \
	goto exit; \
}
		// Wait for wifi
		stoppable(wifi::WifiConnected);
		stoppable(wifi::StmConnected);

		// Init all grabbers
		reload_all();

		while (true) {
			// Clear the refresh grabber flag
			xEventGroupClearBits(wifi::events, wifi::GrabRequested);
			// Wait for wifi to reconnect
			stoppable(wifi::WifiConnected);

			// Run all non-ssl grabbers
			for (size_t i = 0; i < grabber_count; ++i) {
				if (!grabbers[i]->ssl) run_grabber(i, grabbers[i]);
			}

			// Wait for time (so ssl sync works)
			stoppable(wifi::TimeSynced);

			// Run all ssl grabbers
			for (size_t i = 0; i < grabber_count; ++i) {
				if (grabbers[i]->ssl) run_grabber(i, grabbers[i]);
			}

			refresh_for = slots::protocol::GrabberID::NONE;

			// Wait for minimum element
			auto target = *std::min_element(wants_to_run_at, wants_to_run_at + grabber_count);
			auto now = xTaskGetTickCount();
			auto delay = max_delay_between_runs;
			if (target < now || target - now > max_delay_between_runs) {
				ESP_LOGW("grab", "got out of sync in grabber sched (wanted to delay from %u to %u)", now, target);
				if (target < now) {
					delay = 0;
				}
			}
			else delay = target - now;

			if (delay) {
				if (xEventGroupWaitBits(wifi::events, wifi::GrabRequested | wifi::GrabTaskStop, false, false, delay) & wifi::GrabTaskStop) {
					goto exit;
				}
			}
		}

exit:
		dwhttp::close_connection(false);
		dwhttp::close_connection(true);
		ESP_LOGI("grab", "killing done grab task!");
		grabber_task = nullptr;
		vTaskDelete(NULL);
		while (1) {vTaskDelay(portMAX_DELAY);}
	}

	bool is_stopped() { return grabber_task == nullptr; }

	void check_and_restart_grabber_timer(TimerHandle_t handle) {
		static int grab_task_dead_for = 0;

		if (grabber_task != nullptr) return;
		++grab_task_dead_for;
		if (grab_task_dead_for > 100) {
			ESP_LOGE("grab", "grab task dead too long; killing system");
			serial::interface.reset();
			vTaskDelay(1000);
			esp_restart();
		}
		if (xEventGroupGetBits(wifi::events) & wifi::GrabTaskStop) return;
		// try and restart task
		if (xTaskCreate(grabber::run, "grab", 6856, nullptr, 6, &grabber_task) != pdPASS) {
			grabber_task = nullptr;
			ESP_LOGW("grab", "unable to restart grabber task; waiting again");
		}
		else {
			ESP_LOGI("grab", "started grabber task");
			grab_task_dead_for = 0;
		}
	}

	void start() {
		serial::interface.update_slot_nosync(slots::ESP_VER_STR,
#ifndef SIM
					esp_ota_get_app_description()->version
#else
					"fakesim"
#endif
		);

		memset(wants_to_run_at, 0, sizeof(wants_to_run_at));
		auto tmr = xTimerCreate("gtmr", pdMS_TO_TICKS(3000), true, nullptr, check_and_restart_grabber_timer);
		if (tmr == nullptr || xTimerStart(tmr, pdMS_TO_TICKS(2500)) != pdPASS) {
			ESP_LOGE("grab", "unable to start grabber timer...");
			vTaskDelay(1000);
			esp_restart();
		}
	}
};

