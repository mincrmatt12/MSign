#include "sccfg.h"
#include "serial.h"
#include "config.h"
#include <string.h>
#include "time.h"
#include <Arduino.h>
#include <Time.h>

namespace sccfg {
	const uint8_t number_of_screens = 4;

	uint16_t force_enabled_mask = 0;
	uint16_t force_disabled_mask = 0;
	uint16_t parsed_enabled_mask = (1 << number_of_screens) - 1;

	struct Event {
		uint8_t pos;
		bool enable;
		uint32_t timeat;
		bool done_today;
	} parsed_events[number_of_screens * 2 + 1];

	uint16_t parsed_event_count = 0;
	uint8_t  send_idx = 0;

	slots::ScCfgTime times[number_of_screens];

	uint32_t current_timeat() {
		return signtime::get_time() % 86400000;
	}

	void do_event(Event& e) {
		Serial1.printf_P(PSTR("time %d; en=%d\n"), e.timeat, e.enable);
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
			Serial1.println(F("too many onoff events"));
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
					Serial1.println(F("invalid format for entry"));
					return;
				}
				int tA, tB;

				parsed_enabled_mask &= ~(1 << pos);

				sscanf(val, "%d,%d", &tA, &tB);
				append_event({pos, true, static_cast<uint32_t>(tA), false});
				append_event({pos, false, static_cast<uint32_t>(tB), false});
			}
		}
	}

	uint16_t last_enabled_mask = 0;

	void send_mask() {
		uint16_t enabled_mask = (parsed_enabled_mask | force_enabled_mask) & ~force_disabled_mask;
		if (enabled_mask != last_enabled_mask) {
			last_enabled_mask = enabled_mask;

			slots::ScCfgInfo obj;
			obj.display_on = true;
			obj.enabled_mask = last_enabled_mask;

			Serial1.printf("enabled data: %d\n", last_enabled_mask);

			serial::interface.update_data(slots::SCCFG_INFO, (uint8_t *)&obj, sizeof(obj));
		}
	}
	
	void init() {
		create_events_for(0, config::SC_TTC);
		create_events_for(1, config::SC_WEATHER);
		create_events_for(2, config::SC_3D);
		create_events_for(3, config::SC_CFIX);

		times[0].idx_order = 0;
		times[1].idx_order = 1;
		times[2].idx_order = 2;
		times[3].idx_order = 3;

		if (config::manager.get_value(config::SC_TIMES) != nullptr) {
			int tTTC, tWEA, t3D, tCFIX = 12000;

			sscanf(config::manager.get_value(config::SC_TIMES), "%d,%d,%d,%d", &tTTC, &tWEA, &t3D, &tCFIX);

			times[0].millis_enabled = tTTC;
			times[1].millis_enabled = tWEA;
			times[2].millis_enabled = t3D;
			times[3].millis_enabled = tCFIX;
		}
		else {
			times[0].millis_enabled = 12000;
			times[1].millis_enabled = 12000;
			times[2].millis_enabled = 12000;
			times[3].millis_enabled = 12000;
		}

		serial::interface.register_handler([](uint16_t data_id){
			if (data_id == slots::SCCFG_INFO) {
				send_mask();
			}
		});

		serial::interface.register_handler([](uint16_t data_id, uint8_t * buffer, uint8_t & length){
				if (data_id == slots::SCCFG_TIMING) {
					Serial1.printf("sending scc %d\n", send_idx);

					memcpy(buffer, &times[send_idx++], sizeof(times[0]));				
					send_idx %= number_of_screens;
					length = sizeof(times[0]);

					return true;
				}
				return false;
		});
		Serial1.println(F("c?"));
	}

	uint64_t last_run_time = 0;

	void loop() {
		if (parsed_event_count == 0) return;
		
		if (last_run_time > current_timeat()) {
			for (int i = 0; i < parsed_event_count; ++i) {
				parsed_events[i].done_today = false;
			}
		}

		if (last_run_time == 0 && now() < 100) return;
		if (last_run_time == 0) {
			Serial1.println(F("initing sc"));
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

			last_enabled_mask = ~parsed_enabled_mask;
			send_mask();
		}

		last_run_time = current_timeat();

		for (int i = 0; i < parsed_event_count; ++i) {
			if (!parsed_events[i].done_today && parsed_events[i].timeat <= last_run_time) {
				do_event(parsed_events[i]);
			}
		}

		send_mask();
	}

}
