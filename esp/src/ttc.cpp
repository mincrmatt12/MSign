#include "ttc.h"
#include "json.h"
#include "WiFiClient.h"
#include "serial.h"
#include "time.h"
#include "TimeLib.h"
#include "wifi.h"
#include <inttypes.h>
#include "config.h"
#include "string.h"
#include "util.h"

slots::TTCInfo ttc::info;
slots::TTCTime ttc::times[3];

uint64_t       ttc::time_since_last_update = 0;

void ttc::init() {
	memset(ttc::times, 0, sizeof(ttc::times));
    ttc::info = { 0 };

	serial::interface.register_handler(&ttc::on_open);
}

void ttc::loop() {
	if ((now() - time_since_last_update > 15 || time_since_last_update == 0) && wifi::available()) {
		// update the ttc times
		ttc::info.flags = 0;
		for (uint8_t slot = 0; slot < 3; ++slot) {
			const char * stopid = config::manager.get_value((config::Entry)(config::STOPID1 + slot));
			if (stopid != nullptr) {
				memset(&ttc::times[slot], 0, sizeof(ttc::times[0]));
				const char * dtag = config::manager.get_value((config::Entry)(config::DTAG1 + slot));
				const char * name = config::manager.get_value((config::Entry)(config::SNAME1 + slot));
				serial::interface.update_data(slots::TTC_NAME_1 + slot, (const uint8_t *)name, strlen(name));
				ttc::info.nameLen[slot] = strlen(name);

				do_update(stopid, dtag, slot);
			}
		}

		serial::interface.update_data(slots::TTC_INFO, (const uint8_t *)&info, sizeof(info));
		time_since_last_update = now();
	}
}

void ttc::on_open(uint16_t data_id) {
	Serial1.println("onopen: ");
	Serial1.println(data_id);
	switch (data_id) {
		case slots::TTC_INFO:
			serial::interface.update_data(slots::TTC_INFO, (const uint8_t *)&info, sizeof(info));
			break;
		case slots::TTC_TIME_1:
		case slots::TTC_TIME_2:
		case slots::TTC_TIME_3:
			serial::interface.update_data(data_id, (const uint8_t *)(&times[data_id - slots::TTC_TIME_1]), sizeof(slots::TTCTime));
			break;
		case slots::TTC_NAME_1:
		case slots::TTC_NAME_2:
		case slots::TTC_NAME_3:
			{
				const char * name = config::manager.get_value((config::Entry)(config::SNAME1 + (data_id - slots::TTC_NAME_1)));
				if (name == nullptr) return;
				serial::interface.update_data(data_id, (const uint8_t *)name, strlen(name));
				break;
			}
	}
}

void ttc::do_update(const char * stop, const char * dtag, uint8_t slot) {
	char url[80];
	snprintf(url, 80, "/service/publicJSONFeed?command=predictions&a=ttc&stopId=%s", stop);

	Serial1.println(url);
	int16_t status_code;
	auto cb = util::download_with_callback("webservices.nextbus.com", url, status_code);

	if (status_code < 200 || status_code > 299) {
		util::stop_download();
		return;
	}

	// message is here now read it
	
	struct State {
		uint8_t e1;
		uint8_t e2;

		uint64_t epoch = 0;
		bool layover = false;
		bool tag = false;
	} state;

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
					// mark as delayed
					state.layover = true;
				}
			}
			if (strcmp(top.name, "dirTag") == 0 && v.type == json::Value::STR)
			{
				strcpy(dirtag, dtag);
				char *test_str = strtok(dirtag, ",");
				while (test_str != NULL) {
					Serial1.println(test_str);
					Serial1.println(v.str_val);
					if (strcmp(test_str, v.str_val) == 0) {
						state.tag = true; break;
					}
					else {
						test_str = strtok(NULL, ",");
					}
				}
			}
			if (strcmp(top.name, "epochTime") == 0 && v.type == json::Value::STR) {
				state.epoch = time::millis_to_local(atoll(v.str_val));
			}
		}
		else if (top.is_array() && strcmp(top.name, "prediction") == 0 && v.type == json::Value::OBJ) {
			if (state.tag && state.e2 < 2) {
				ttc::info.flags |= (slots::TTCInfo::EXIST_0 << slot);
				if (state.epoch < ttc::times[slot].tA || ttc::times[slot].tA == 0) {
					ttc::times[slot].tB = ttc::times[slot].tB;
					ttc::times[slot].tA = state.epoch;
					if (state.layover) {
						ttc::info.flags |= (slots::TTCInfo::DELAY_0 << slot);
					}
				}
				else if (state.epoch < ttc::times[slot].tB || ttc::times[slot].tB == 0) {
					ttc::times[slot].tB = state.epoch;
					if (state.layover) {
						ttc::info.flags |= (slots::TTCInfo::DELAY_0 << slot);
					}
				}

				on_open(slots::TTC_TIME_1 + slot);

				Serial1.print(F("Adding ttc entry in slot "));
				Serial1.print(slot);
			}
			state.tag = false;
			state.layover = false;
			state.epoch = 0;
		}
	});

	if (!parser.parse(std::move(cb))) {
		Serial1.println(F("JSON fucked up."));
	} // parse while calling our function.

	free(dirtag);
}
