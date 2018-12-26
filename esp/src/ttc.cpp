#include "ttc.h"
#include "HttpClient.h"
#include "json.h"
#include "WiFiClient.h"
#include "serial.h"
#include "TimeLib.h"
#include "wifi.h"
#include "config.h"

WiFiClient client;
HttpClient http_client(client);

void ttc::init() {
	memset(ttc::times, 0, sizeof(ttc::times));
    ttc::info = { 0 };

	serial::interface.register_handler((serial::OpenHandler)&ttc::on_open);
}

void ttc::loop() {
	if ((now() - time_since_last_update > 15 || time_since_last_update == 0) && wifi::available()) {
		// update the ttc times
		ttc::info.flags = 0;
		for (uint8_t slot = 0; slot < 3; ++slot) {
			const char * stopid = config::manager.get_value((config::Entry)(config::STOPID1 + slot));
			if (stopid != nullptr) {
				const char * dtag = config::manager.get_value((config::Entry)(config::DTAG1 + slot));
				const char * name = config::manager.get_value((config::Entry)(config::SNAME1 + slot));
				serial::interface.update_data(slots::TTC_NAME_1 + slot, (const uint8_t *)name, strlen(name));
				ttc::info.nameLen[slot] = strlen(name);

				do_update(stopid, dtag, slot);
			}
		}

		serial::interface.update_data(slots::TTC_INFO, (const uint8_t *)&info, sizeof(info));
	}
}

void ttc::on_open(uint16_t data_id) {
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
				serial::interface.update_data(data_id, (const uint8_t *)name, strlen(name));
				break;
			}
	}
}

void ttc::do_update(const char * stop, const char * dtag, uint8_t slot) {
	{
		char url[80];
		snprintf(url, 80, "/service/publicJSONFeed?command=predictions&a=ttc&stopId=%s", stop);

		if (http_client.get("webservices.nextbus.com", url) != 0) return;
	}
	
	if (http_client.responseStatusCode() != 200) {http_client.stop(); return;}

	http_client.skipResponseHeaders();
	int body_length = http_client.contentLength();

	char body[body_length];

	char * pos = body;
	uint64_t ts = millis();
	while ((http_client.connected() || http_client.available()) && (millis() - ts) < 100) {
		if (http_client.available()) {
			int size = http_client.available();
			http_client.read((uint8_t *)pos, size);
			pos += size;
			if (pos - body >= body_length) break;
			ts = millis();
		}
		else {
			delay(1);
		}
	}

	if (millis() - ts < 100) return;

	http_client.stop();

	// message is here now read it
	
	struct State {
		uint8_t e1;
		uint8_t e2;

		uint64_t epoch = 0;
		bool layover = false;
		bool tag = false;
	} state;
	
	json::JSONParser parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
		if (stack_ptr < 2) return;

		json::PathNode &top = *stack[stack_ptr-1];
		json::PathNode &parent = *stack[stack_ptr-2];

		uint8_t e1 = stack[1]->index;
		uint8_t e2 = parent.index;

		if ((state.e1 != e1 || state.e2 != e2) && state.tag) {
			// apply it here
			
			if (state.e2 < 2) {
				if (state.layover) {
					ttc::info.flags |= (slots::TTCInfo::DELAY_0 << slot);
				}
				if (state.e2 == 0) {
					ttc::times[slot].tA = state.epoch;
				}
				else {
					ttc::times[slot].tB = state.epoch;
				}

				on_open(slots::TTC_TIME_1 + slot);
			}

			// update
			state.tag = false;
			state.e1 = e1;
			state.e2 = e2;
		}

		if (parent.is_array() && strcmp(parent.name, "prediction") == 0) {
			// use the first two only
			
			if (strcmp(top.name, "affectedByLayover") == 0) {
				// is it's value true?
				if (v.type == json::Value::STR && strcmp(v.str_val, "true") == 0) {
					// mark as delayed
					state.layover = true;
				}
			}
			if (strcmp(top.name, "dirTag") == 0 && v.type == json::Value::STR && strcmp(v.str_val, dtag))
				state.tag = true;
			
			if (strcmp(top.name, "epochTime") == 0 && v.type == json::Value::STR) {
				sscanf(v.str_val, "%llu", &state.epoch);
			}
		}
	});

	parser.parse(body, body_length);

	if (state.tag && state.e2 < 2) {
		if (state.layover) {
			ttc::info.flags |= (slots::TTCInfo::DELAY_0 << slot);
		}
		if (state.e2 == 0) {
			ttc::times[slot].tA = state.epoch;
		}
		else {
			ttc::times[slot].tB = state.epoch;
		}

		on_open(slots::TTC_TIME_1 + slot);
	}
}
