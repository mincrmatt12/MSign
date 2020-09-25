#include "ttc.h"
#include "../json.h"
#include "../serial.h"
#include "../util.h"
#include "../wifitime.h"
#include "../config.h"

#include <algorithm>
#include <esp_log.h>

const static char * TAG = "ttc";

namespace ttc {
	void init() {
		// Initialize all the slot names
		for (uint8_t slot = 0; slot < 3; ++slot) {
			if (config::manager.get_value((config::Entry)(config::STOPID1 + slot))) {
				const char * name = config::manager.get_value((config::Entry)(config::SNAME1 + slot));
				serial::interface.update_slot(slots::TTC_NAME_1 + slot, name);
			}
		}
	}

	// Populate the info and times with the information for this slot.
	bool update_slot_times_and_info(const char * stop, const char * dtag, uint8_t slot, slots::TTCInfo& info, uint64_t times[6]) {
		char url[80];
		snprintf(url, 80, "/service/publicJSONFeed?command=predictions&a=ttc&stopId=%s", stop);

		int16_t status_code;
		auto cb = util::download_with_callback("webservices.nextbus.com", url, status_code);

		if (status_code < 200 || status_code > 299) {
			util::stop_download();
			return false;
		}

		ESP_LOGD(TAG, "Parsing json data");

		// message is here now read it
		
		struct State {
			uint8_t e1;
			uint8_t e2;

			uint64_t epoch = 0;
			bool layover = false;
			bool tag = false;
		} state;

		int max_e = 0;

		char * dirtag = strdup(dtag);
		
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
					strcpy(dirtag, dtag);
					char *test_str = strtok(dirtag, ",");
					while (test_str != NULL) {
						if (strcmp(test_str, v.str_val) == 0) {
							ESP_LOGD(TAG, "matched on %s", test_str);
							state.tag = true; break;
						}
						else {
							test_str = strtok(NULL, ",");
						}
					}
				}
				if (strcmp(top.name, "epochTime") == 0 && v.type == json::Value::STR) {
					state.epoch = wifi::millis_to_local(atoll(v.str_val));
					ESP_LOGD(TAG, "got epochtime %d", (int)(state.epoch / 1000));
				}
			}
			else if (top.is_array() && strcmp(top.name, "prediction") == 0 && v.type == json::Value::OBJ) {
				if (state.tag && state.e2 < 6) {
					info.flags |= (slots::TTCInfo::EXIST_0 << slot);
					if (state.layover) {
						info.flags |= (slots::TTCInfo::DELAY_0 << slot);
					}
					if (max_e < 6) {
						ESP_LOGD(TAG, "Got an entry at %d, idx=%d", (int)(state.epoch/1000), max_e);
						times[max_e++] = state.epoch;
					}
				}
				state.tag = false;
				state.layover = false;
				state.epoch = 0;
			}
		});

		bool ok = true;

		if (!parser.parse(std::move(cb))) {
			ok = false;
			ESP_LOGD(TAG, "Parse failed");
		} // parse while calling our function.

		util::stop_download();
		free(dirtag);

		if (max_e)
			std::sort(times, times+max_e);

		return ok;
	}

	bool loop() {
		slots::TTCInfo x{};
		for (uint8_t slot = 0; slot < 3; ++slot) {
			uint64_t local_times[6];
			memset(local_times, 0, sizeof(local_times));

			const char * stopid = config::manager.get_value((config::Entry)(config::STOPID1 + slot));
			if (stopid != nullptr) {
				const char * dtag = config::manager.get_value((config::Entry)(config::DTAG1 + slot));
				if (update_slot_times_and_info(stopid, dtag, slot, x, local_times)) {
					serial::interface.update_slot(slots::TTC_TIME_1 + slot, local_times, sizeof(local_times));
				}
				else return false;
			}
		}
		serial::interface.update_slot(slots::TTC_INFO, x);
		// TODO: alertstr
		return true;
	}
}
