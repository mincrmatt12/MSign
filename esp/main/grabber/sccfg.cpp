#include "sccfg.h"
#include "../serial.h"
#include "../config.h"
#include <string.h>
#include "../wifitime.h"
#include "esp_log.h"
#include "sccfg.cfg.h"

const static char * TAG = "sccfg";

namespace sccfg {

	uint16_t force_enabled_mask = 0;
	uint16_t force_disabled_mask = 0;
	uint16_t parsed_enabled_mask = (1 << number_of_screens) - 1;

	struct Event {
		int32_t timeat;
		uint8_t pos;
		bool enable;
		bool done_today;
	} parsed_events[number_of_screens * 2 + 1];

	uint16_t parsed_event_count = 0;

	uint32_t current_timeat() {
		return wifi::get_localtime() % 86400000;
	}

	void do_event(Event& e) {
		ESP_LOGD(TAG, "time %d; en=%d", e.timeat, e.enable);
		if (e.enable) {
			parsed_enabled_mask |= (1 << e.pos);
		}
		else {
			parsed_enabled_mask &= ~(1 << e.pos);
		}

		e.done_today = true;
	}

	void append_event(Event e) {
		if (parsed_event_count > number_of_screens * 2) {
			ESP_LOGD(TAG, "too many onoff events");
			return;
		}
		
		parsed_events[parsed_event_count++] = e;
	}

	void send_mask() {
		uint16_t enabled_mask = (parsed_enabled_mask | force_enabled_mask) & ~force_disabled_mask;
		slots::ScCfgInfo obj;
		obj.display_on = true;
		obj.enabled_mask = enabled_mask;

		ESP_LOGD(TAG, "enabled data: %d", enabled_mask);

		serial::interface.update_slot(slots::SCCFG_INFO, obj);
	}
	
	void init() {
		parsed_enabled_mask = 0;

		slots::ScCfgTime times[number_of_screens];
		int number_configured = 0;

		for (; number_configured < number_of_screens && screen_entries[number_configured]; ++number_configured) {
			// Set enabled
			parsed_enabled_mask |= 1 << screen_entries[number_configured]->screen;
			// Add into order
			times[number_configured].screen_id = screen_entries[number_configured]->screen;
			times[number_configured].millis_enabled = screen_entries[number_configured]->duration;
			// Add events
			if (!screen_entries[number_configured]->times.always_on()) {
				append_event({screen_entries[number_configured]->times.start, screen_entries[number_configured]->screen, true, false});
				append_event({screen_entries[number_configured]->times.end, screen_entries[number_configured]->screen, true, false});
			}
		}

		serial::interface.update_slot_raw(slots::SCCFG_TIMING, &times[0], sizeof(slots::ScCfgTime)*number_configured);
	}

	uint64_t last_run_time = 0;

	bool loop() {
		if (last_run_time > current_timeat()) {
			for (int i = 0; i < parsed_event_count; ++i) {
				parsed_events[i].done_today = false;
			}
		}

		if (last_run_time == 0) {
			ESP_LOGD(TAG, "initing sc");
			uint64_t last_timeat_d = 0;
			for (int i = 0; i < parsed_event_count; ++i) {
				uint64_t min_value_sofar = 8.64e7 + 1;
				int k = 0;
				for (int j = 0; j < parsed_event_count; ++j) {
					if (parsed_events[j].timeat <= last_timeat_d) continue;
					if (parsed_events[j].timeat < min_value_sofar) {
						k = j;
						min_value_sofar = parsed_events[j].timeat;
					}
				}
				last_timeat_d = min_value_sofar;
				if (parsed_events[k].timeat <= current_timeat()) do_event(parsed_events[k]);
			}
		}

		last_run_time = current_timeat();

		for (int i = 0; i < parsed_event_count; ++i) {
			if (!parsed_events[i].done_today && parsed_events[i].timeat <= last_run_time) {
				do_event(parsed_events[i]);
			}
		}

		send_mask();
		return true;
	}

}
