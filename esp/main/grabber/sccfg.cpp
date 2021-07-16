#include "sccfg.h"
#include "../serial.h"
#include "../config.h"
#include <string.h>
#include "../wifitime.h"
#include "esp_log.h"

const static char * TAG = "sccfg";

namespace sccfg {
	const uint8_t number_of_screens = 3;

	uint16_t force_enabled_mask = 0;
	uint16_t force_disabled_mask = 0;
	uint16_t parsed_enabled_mask = (1 << number_of_screens) - 1;

	struct Event {
		uint32_t timeat;
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

	void create_events_for(uint8_t pos, config::Entry entry) {
		if (config::manager.get_value(entry) == nullptr) return;
		else {
			const auto val = config::manager.get_value(entry);
			if (strcmp("on", val) == 0) return;
			if (strcmp("off", val) == 0) {
				parsed_enabled_mask &= ~(1 << pos);
			}
			else {
				if (strchr(val, ',') == nullptr) {
					ESP_LOGD(TAG, "invalid format for entry");
					return;
				}
				int tA, tB;

				parsed_enabled_mask &= ~(1 << pos);

				sscanf(val, "%d,%d", &tA, &tB);
				append_event({static_cast<uint32_t>(tA), pos, true, false});
				append_event({static_cast<uint32_t>(tB), pos, false, false});
			}
		}
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
		create_events_for(0, config::SC_TTC);
		create_events_for(1, config::SC_WEATHER);
		create_events_for(2, config::SC_3D);

		slots::ScCfgTime times[number_of_screens];

		if (config::manager.get_value(config::SC_ORDER)) {
			int indices[4];
			sscanf(config::manager.get_value(config::SC_ORDER), "%d,%d,%d,%d", &indices[0], &indices[1], &indices[2], &indices[3]);

			for (int i = 0; i < 3; ++i) {
				times[i].screen_id = static_cast<slots::ScCfgTime::ScreenId>(indices[i]);
			}
		}
		else {
			times[0].screen_id = slots::ScCfgTime::TTC;
			times[1].screen_id = slots::ScCfgTime::WEATHER;
			times[2].screen_id = slots::ScCfgTime::MODEL;
		}

		if (config::manager.get_value(config::SC_TIMES) != nullptr) {
			int tTTC, tWEA, t3D, tCFIX = 12000;

			sscanf(config::manager.get_value(config::SC_TIMES), "%d,%d,%d,%d", &tTTC, &tWEA, &t3D, &tCFIX);

			times[0].millis_enabled = tTTC;
			times[1].millis_enabled = tWEA;
			times[2].millis_enabled = t3D;
		}
		else {
			times[0].millis_enabled = 12000;
			times[1].millis_enabled = 12000;
			times[2].millis_enabled = 12000;
		}

		serial::interface.update_slot(slots::SCCFG_TIMING, times);
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
