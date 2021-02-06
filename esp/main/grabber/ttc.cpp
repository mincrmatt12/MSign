#include "ttc.h"
#include "../json.h"
#include "../serial.h"
#include "../dwhttp.h"
#include "../wifitime.h"
#include "../config.h"

#include <algorithm>
#include <esp_log.h>

extern "C" {
#include <ttc_rdf.h>
};

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
		// Yes this uses HTTP, and not just because it's possible but because the TTC webservices break if you use HTTPS yes really.
		auto cb = dwhttp::download_with_callback("webservices.nextbus.com", url, status_code);

		if (status_code < 200 || status_code > 299) {
			dwhttp::stop_download();
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
			else if ((top.is_array() || top.is_obj()) && strcmp(top.name, "prediction") == 0 && v.type == json::Value::OBJ) {
				if (state.tag && state.e2 < 6) {
					info.flags |= (slots::TTCInfo::EXIST_0 << slot);
					if (state.layover) {
						info.flags |= (slots::TTCInfo::DELAY_0 << slot);
					}
					if (max_e < 6) {
						ESP_LOGD(TAG, "Got an entry at %d, idx=%d", (int)(state.epoch/1000), max_e);
						times[max_e++] = state.epoch;
						if (max_e == 6) {
							// Sort here too so that the sorted add will work
							std::sort(times, times+6);
						}
					}
					else {
						auto lb = std::lower_bound(times, times+6, state.epoch);
						if (lb < (times + 5)) {
							ESP_LOGD(TAG, "Got sorted entry <5 at %d, idx=%d", (int)(state.epoch/1000), (int)std::distance(times, lb));
							std::move(lb, times+5, lb+1);
							*lb = state.epoch;
						}
						else if (lb != (times + 6)) {
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

		if (!parser.parse(std::move(cb))) {
			ok = false;
			ESP_LOGE(TAG, "Parse failed");
		} // parse while calling our function.

		dwhttp::stop_download();
		free(dirtag);

		// Ensure the array is sorted
		if (max_e && max_e < 6)
			std::sort(times, times+max_e);

		return ok;
	}

	struct AlertParserState {
		slots::TTCInfo& info;
		int offset = 0;
	};

	void update_alertstr(slots::TTCInfo& info) {
		ttc_rdf_state_t state;
		AlertParserState aps_ptr{info};

		int16_t status_code;
		auto cb = dwhttp::download_with_callback("www.ttc.ca", "/RSS/Service_Alerts/index.rss", status_code);

		if (status_code < 200 || status_code > 299) {
			dwhttp::stop_download();
			return;
		}

		ttc_rdf_start(&state);
		state.userptr = &aps_ptr;

		// Run this through the parser
		int16_t result;
		while ((result = cb()) != -1) {
			switch (ttc_rdf_feed((uint8_t)result, false, &state)) {
				case TTC_RDF_OK:
					continue;
				case TTC_RDF_FAIL:
					ESP_LOGE(TAG, "RDF parser failed");
				case TTC_RDF_DONE:
					goto end_parseloop;
			}
		}
end_parseloop:
		dwhttp::stop_download();
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
		update_alertstr(x);
		if (!(x.flags & slots::TTCInfo::SUBWAY_ALERT)) {
			serial::interface.delete_slot(slots::TTC_ALERTSTR);
		}
		serial::interface.update_slot(slots::TTC_INFO, x);
		return true;
	}
}

const static char * xml_entities[] = {
	"&amp;", "&",
	"&apos;", "'",
	nullptr
};

bool replace_xml_ent(char *in, const char* search, const char* repl) {
	char * pos = strstr(in, search);
	if (pos) {
		char * newoff = pos + strlen(repl);
		char * oldoff = pos + strlen(search);
		char * end = in + strlen(in) + 1;

		memmove(newoff, oldoff, end-oldoff);
		memcpy(pos, repl, strlen(repl));
	}
	return pos;
}

extern "C" void ttc_rdf_on_advisory_hook(ttc_rdf_state_t *state, uint8_t inval) {
	ttc::AlertParserState& ps = *reinterpret_cast<ttc::AlertParserState *>(state->userptr);

	ESP_LOGD(TAG, "Got advisory entry %s", state->c.advisory);

	// Check all semicolon separated entries
	{
		char * search_query = strdup(config::manager.get_value(config::ALERT_SEARCH, ""));
		char * token = strtok(search_query, ";");
		bool found = false;
		while (token && strlen(token)) {
			if (strcasestr(state->c.advisory, token)) {
				ESP_LOGD(TAG, "matched on %s", token);
				found = true;
				break;
			}
			token = strtok(NULL, ";");
		}
		free(search_query);
		if (!found) return;
	}

	// Ignore if we see the string elevator in there
	if (strcasestr(state->c.advisory, "elevator")) return;

	// Is the first alert?
	if (!(ps.info.flags & slots::TTCInfo::SUBWAY_ALERT)) {
		// Clear the type flags
		ps.info.flags &= ~(slots::TTCInfo::SUBWAY_DELAYED | slots::TTCInfo::SUBWAY_OFF);
		// Mark this alert.
		ps.info.flags |=   slots::TTCInfo::SUBWAY_ALERT;
	}
	
	// If it isn't a regular service...
	if (!strcasestr(state->c.advisory, "regular service has")) {
		// Try and guess the type of error:
		if (strcasestr(state->c.advisory, "delays of") || strcasestr(state->c.advisory, "major delays") || strcasestr(state->c.advisory, "trains are not stopping") || strcasestr(state->c.advisory, "trains not stopping")) {
			ps.info.flags |= slots::TTCInfo::SUBWAY_DELAYED;
		}
		if (strcasestr(state->c.advisory, "no service")) {
			ps.info.flags |= slots::TTCInfo::SUBWAY_OFF;
		}
	}

	// Try and replace all known xml entities
	while (true) {
		bool any = false;
		for (const char ** i = &xml_entities[0]; i[0] != nullptr; i += 2) {
			any |= replace_xml_ent(state->c.advisory, i[0], i[1]);
		}
		if (!any) break;
	}

	size_t newlength = strlen(state->c.advisory);
	// Add to the slot data:
	// If this is the first entry, we don't need to add a ' / ' marker, otherwise we do
	if (ps.offset != 0) newlength += 3;
	serial::interface.allocate_slot_size(slots::TTC_ALERTSTR, ps.offset + newlength + 1);
	if (ps.offset == 0) {
		serial::interface.update_slot_partial(slots::TTC_ALERTSTR, 0, state->c.advisory, strlen(state->c.advisory)+1);
	}
	else {
		serial::interface.update_slot_partial(slots::TTC_ALERTSTR, ps.offset + 3, state->c.advisory, strlen(state->c.advisory)+1);
		serial::interface.update_slot_partial(slots::TTC_ALERTSTR, ps.offset, " / ", 3);
	}
	ps.offset += newlength;
}
