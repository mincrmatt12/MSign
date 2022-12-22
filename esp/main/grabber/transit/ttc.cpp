#include "ttc.h"
#include "../../json.h"
#include "../sccfg.h"
#include "../../serial.h"
#include "../../dwhttp.h"
#include "../../wifitime.h"
#include "ttc.cfg.h"
#include "common.cfg.h"

#include <algorithm>
#include <esp_log.h>

const static char * const TAG = "ttc";

namespace transit::ttc {
	void init() {
		if (impl == TTC) load_static_info("ttc");
	}

	// Populate the info and times with the information for this slot.
	bool update_slot_times_and_info(const TTCEntry& entry, uint8_t slot, slots::TTCInfo& info, uint64_t times[6], uint64_t times_b[6]) {
		char url[80];
		snprintf(url, 80, "/service/publicJSONFeed?command=predictions&a=%s&stopId=%d", agency_code.get(), entry.stopid);

		// Yes this uses HTTP, and not just because it's possible but because the TTC webservices break if you use HTTPS yes really.
		auto dw = dwhttp::download_with_callback("_retro.umoiq.com", url);

		if (dw.result_code() < 200 || dw.result_code() > 299) {
			return false;
		}

		dw.make_nonclose();

		ESP_LOGD(TAG, "Parsing json data");
		// message is here now read it
		
		struct State {
			uint8_t e1;
			uint8_t e2;

			uint64_t epoch = 0;
			bool layover = false;
			int tag = 0;
		} state;

		int max_e = 0, max_e_alt = 0;
		
		json::JSONParser parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
			if (stack_ptr < 2) return;

			json::PathNode &top = *stack[stack_ptr-1];
			json::PathNode &parent = *stack[stack_ptr-2];

			if ((parent.is_array() || parent.is_obj()) && strcmp(parent.name, "prediction") == 0) {
				// use the first two only

				state.e1 = stack[1]->index;
				state.e2 = parent.index;
				
				if (strcmp(top.name, "affectedByLayover") == 0) {
					// is it's value true?
					if (v.type == json::Value::STR && strcmp(v.str_val, "true") == 0) {
						ESP_LOGD(TAG, "affectedByLayover");
						// mark as delayed
						state.layover = true;
					}
					else {
						state.layover = false;
					}
				}
				if (strcmp(top.name, "dirTag") == 0 && v.type == json::Value::STR)
				{
					for (int i = 0; i < 4 && entry.dirtag[i]; ++i) {
						if (entry.dirtag[i] == v.str_val) {
							ESP_LOGD(TAG, "matched on %s", entry.dirtag[i].get());
							state.tag = 1; break;
						}
					}
					for (int i = 0; i < 4 && entry.alt_dirtag[i]; ++i) {
						if (entry.alt_dirtag[i] == v.str_val) {
							ESP_LOGD(TAG, "matched on %s (alt)", entry.alt_dirtag[i].get());
							state.tag = 2; break;
						}
					}
				}
				if (strcmp(top.name, "epochTime") == 0 && v.type == json::Value::STR) {
					state.epoch = wifi::millis_to_local(atoll(v.str_val));
					ESP_LOGD(TAG, "got epochtime %d", (int)(state.epoch / 1000));
				}
			}
			else if ((top.is_array() || top.is_obj()) && strcmp(top.name, "prediction") == 0 && v.type == json::Value::OBJ) {
				if (state.tag && state.e2 < 6) {
					info.flags |= (slots::TTCInfo::EXIST_0 << slot);
					//if (state.layover) {
						//info.flags |= (slots::TTCInfo::DELAY_0 << slot);
					//}
					auto& relevant_length = (state.tag == 1 ? max_e : max_e_alt);
					auto& relevant_times  = (state.tag == 1 ? times : times_b);
					if (relevant_length < 6) {
						ESP_LOGD(TAG, "Got an entry at %d, idx=%d", (int)(state.epoch/1000), relevant_length);
						relevant_times[relevant_length++] = state.epoch;
						if (relevant_length == 6) {
							// Sort here too so that the sorted add will work
							std::sort(relevant_times, relevant_times+6);
						}
					}
					else {
						auto lb = std::lower_bound(relevant_times, relevant_times+6, state.epoch);
						if (lb < (relevant_times + 5)) {
							ESP_LOGD(TAG, "Got sorted entry <5 at %d, idx=%d", (int)(state.epoch/1000), (int)std::distance(relevant_times, lb));
							std::move(lb, relevant_times+5, lb+1);
							*lb = state.epoch;
						}
						else if (lb != (relevant_times + 6)) {
							ESP_LOGD(TAG, "Got sorted entry =6 at %d", (int)(state.epoch/1000));
							*lb = state.epoch;
						}
					}
				}
				state.tag = false;
				state.layover = false;
				state.epoch = 0;
			}
		});

		bool ok = true;

		if (!parser.parse(dw)) {
			ok = false;
			ESP_LOGE(TAG, "Parse failed");
		} // parse while calling our function.

		// Ensure the array is sorted
		if (max_e && max_e < 6)
			std::sort(times, times+max_e);

		if (max_e_alt && max_e_alt < 6)
			std::sort(times_b, times+max_e_alt);

		return ok;
	}

	struct AlertParserState {
		slots::TTCInfo& info;
		int offset = 0;
	};

	bool loop() {
		if (transit::impl != transit::TTC) return true;

		bool did_get_any = false;
		slots::TTCInfo x{};
		for (uint8_t slot = 0; slot < 5; ++slot) {
			uint64_t local_times[6], local_times_b[6];
			memset(local_times, 0, sizeof(local_times));
			memset(local_times_b, 0, sizeof(local_times_b));

			if (entries[slot] && *entries[slot]) {
				if (update_slot_times_and_info(*entries[slot], slot, x, local_times, local_times_b)) {
					if (local_times[0]) {
						serial::interface.update_slot_raw(slots::TTC_TIME_1a + slot, local_times, sizeof(uint64_t) * std::count_if(std::begin(local_times), std::end(local_times), [](auto x){return x != 0;}));
						did_get_any = true;
					}
					if (local_times_b[0]) {
						serial::interface.update_slot_raw(slots::TTC_TIME_1b + slot, local_times_b, sizeof(uint64_t) * std::count_if(std::begin(local_times_b), std::end(local_times_b), [](auto x){return x != 0;}));
						did_get_any = true;
					}

					// set slot altcodes
					x.altdircodes_a[slot] = (char)entries[slot]->dir_a;
					x.altdircodes_b[slot] = (char)entries[slot]->dir_b;
					x.stopdistance[slot] = entries[slot]->distance_minutes;
				}
				else return false;
			}
		}
		dwhttp::close_connection(true);
		if (!(x.flags & slots::TTCInfo::SUBWAY_ALERT)) {
			serial::interface.delete_slot(slots::TTC_ALERTSTR);
		}
		serial::interface.update_slot(slots::TTC_INFO, x);
		sccfg::set_force_disable_screen(slots::ScCfgInfo::TTC, !did_get_any);
		return true;
	}
}
