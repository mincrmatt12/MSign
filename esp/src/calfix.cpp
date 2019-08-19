#include "calfix.h"
#include "serial.h"
#include "wifi.h"
#include "util.h"
#include "json.h"
#include "common/slots.h"
#include <Time.h>

namespace calfix {
	uint8_t current_day = 0;
	uint8_t current_schedule = 0;
	slots::CalfixInfo calfix_info;
	slots::ClassInfo p1, p2, p3, p4;
	slots::PeriodInfo prdh1, prdh2;
	uint64_t time_since_last_update = 0;

	json::JSONParser c_parser([](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v) {
		if (stack_ptr == 4 && strncmp(stack[1]->name, "sched", 5) == 0 && stack[1]->name[5] == '0' + current_schedule) {
			int period = stack[2]->index;
			auto &prdh = (period < 2) ? prdh1 : prdh2;
			if (stack[3]->index == 0)
				(period % 2 == 0) ? prdh.ps1 : prdh.ps2 = v.int_val  * 60 * 60 * 1000;
			else
				(period % 2 == 0) ? prdh.ps1 : prdh.ps2 += v.int_val * 60 * 1000;
		}
		else if (stack_ptr == 4 && strncmp(stack[1]->name, "day", 3) == 0 && stack[1]->name[3] == '0' + current_day) {
			int period = stack[2]->index;
			auto pinf = (period > 0) ? ((period > 1) ? ((period == 2) ? p3 : p4) : p2) : p1;
			if (strcmp(stack[3]->name, "name") == 0) strcpy((char *)pinf.name, v.str_val);
			else if (strcmp(stack[3]->name, "loc") == 0) {
				sscanf(v.str_val, "%hu", &pinf.room);
			}
		}
	});

	void get_currentdata() {
		int16_t status_code;
		auto cb = util::download_with_callback("calfix.i.mm12.xyz", "/state.json", status_code); // leading _ indicates https

		if (status_code < 200 || status_code >= 300) {
			util::stop_download();
			return;
		}

		memset(&p1, 0, sizeof p1);
		memset(&p2, 0, sizeof p2);
		memset(&p3, 0, sizeof p3);
		memset(&p4, 0, sizeof p4);

		c_parser.parse(std::move(cb));
		util::stop_download();
	}

	bool ran_today() {return current_day > 0;}

	bool get_currentday() {
		util::Download dwn = util::download_from("calfix.i.mm12.xyz", "/currentday.txt");

		if (dwn.status_code != 200) {
			Serial1.println(F("invalid response on dwn"));

			return false;
		}

		char strbuf[dwn.length + 1];
		if (dwn.buf) {
			strncpy(strbuf, dwn.buf, dwn.length);
			free(dwn.buf);
			strbuf[dwn.length] = 0;
		}
		else {
			Serial1.println(F("invalid return length."));

			return false;
		}

		int i1, i2;
		if (!sscanf(strbuf, "%d %d", &i1, &i2)) {
			Serial1.println(strbuf);
			Serial1.println(F("invalid format"));
		}

		current_day = i1;
		current_schedule = i2;

		Serial1.println(current_day);

		calfix_info.day = current_day;
		calfix_info.abnormal = current_schedule != 1;
		calfix_info.active = ran_today();

		return true;
	}

	void init() {
		// get the init ready
		
		// blargh
		
		memset(&p1, 0, sizeof p1);
		memset(&p2, 0, sizeof p2);
		memset(&p3, 0, sizeof p3);
		memset(&p4, 0, sizeof p4);

		serial::interface.register_handler([](uint16_t data_id){
			switch (data_id) {
				case slots::CALFIX_INFO:
					serial::interface.update_data(data_id, (uint8_t *)&calfix_info, sizeof calfix_info);
					break;
				case slots::CALFIX_CLS1:
				case slots::CALFIX_CLS2:
				case slots::CALFIX_CLS3:
				case slots::CALFIX_CLS4:
					{
						int period = data_id - slots::CALFIX_CLS1;
						serial::interface.update_data(data_id, (uint8_t *)&((period > 0) ? ((period > 1) ? ((period == 2) ? p3 : p4) : p2) : p1), sizeof p1);
					}
					break;
				case slots::CALFIX_PRDH1:
					serial::interface.update_data(data_id, (uint8_t *)&prdh1, sizeof prdh1);
					break;
				case slots::CALFIX_PRDH2:
					serial::interface.update_data(data_id, (uint8_t *)&prdh2, sizeof prdh1);
					break;
			}
		});
	}

	void loop() {
		if (now() < 100) return;
		if (time_since_last_update == 0 || (now() - time_since_last_update) > 14400) {
			if (get_currentday()) {
				serial::interface.update_data(slots::CALFIX_INFO, (uint8_t *)&calfix_info, sizeof calfix_info);
				if (ran_today()) {
					get_currentdata();

					serial::interface.update_data(slots::CALFIX_CLS1, (uint8_t *)&p1, sizeof p1);
					serial::interface.update_data(slots::CALFIX_CLS2, (uint8_t *)&p2, sizeof p2);
					serial::interface.update_data(slots::CALFIX_CLS3, (uint8_t *)&p3, sizeof p3);
					serial::interface.update_data(slots::CALFIX_CLS4, (uint8_t *)&p4, sizeof p4);

					serial::interface.update_data(slots::CALFIX_PRDH1, (uint8_t *)&prdh1, sizeof prdh1);
					serial::interface.update_data(slots::CALFIX_PRDH2, (uint8_t *)&prdh2, sizeof prdh2);
				}

				time_since_last_update = now();
			}
			else {
				Serial1.println(F("failed to req."));
			}
		}
	}
}
