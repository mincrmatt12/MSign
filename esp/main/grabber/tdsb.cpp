#include "tdsb.h"
#include "../dwhttp.h"
#include "../config.h"
#include "../json.h"
#include <esp_log.h>
#include <memory>
#include <ctime>
#include "../common/slots.h"
#include "../serial.h"

extern "C" {
#include <tdsb_logintoken.h>
};

const static char* TAG = "tdsb";

namespace tdsb {

	void init() {
	}

	// This shouldn't be called for a day "0"; which is always empty
	bool check_layout_for(
		json::TextCallback&& tcb,
		slots::TimetableHeader::LayoutStyle &layout_out
	) {
		layout_out = slots::TimetableHeader::EMPTY;

		bool at_home = false;
		bool am = false;

		json::JSONParser jp([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value &val){
			if (stack_ptr >= 3 && stack[1]->is_array() && !strcmp(stack[1]->name, "CourseTable") && !strcmp(stack[2]->name, "StudentCourse")) {
				if (stack_ptr == 4 && !strcmp(stack[3]->name, "Period") && val.type == val.STR) {
					am = val.str_val[0] == '1';
				}
				else if (stack_ptr == 4 && !strcmp(stack[3]->name, "RoomNo") && val.type == val.STR) {
					at_home = !strcmp(val.str_val, "*AT HOME");
				}
				else if (stack_ptr == 3 && val.type == val.OBJ) {
					// AM is always before PM, so if we see AM we just set directly
					
					ESP_LOGD(TAG, "got course at_home=%d am=%d curr_layout=%d", at_home, am, layout_out);

					if (am) {
						if (at_home) layout_out = slots::TimetableHeader::AM_ONLY;
						else         layout_out = slots::TimetableHeader::AM_INCLASS_ONLY;
					}
					else {
						// Otherwise check if layout_out has been set
						switch (layout_out) {
							case slots::TimetableHeader::EMPTY:
								layout_out = slots::TimetableHeader::PM_ONLY;
								break;
							case slots::TimetableHeader::AM_INCLASS_ONLY:
								layout_out = slots::TimetableHeader::AM_INCLASS_PM;
								break;
							case slots::TimetableHeader::AM_ONLY:
								layout_out = slots::TimetableHeader::AM_PM;
								break;
							default:
								break;
						}
					}
				}
			}
		});
		
		return jp.parse(std::move(tcb));
	}

	bool loop() {
		 // Try to log in
		
		if (!config::manager.get_value(config::TDSB_USER) || !config::manager.get_value(config::TDSB_PASS)) {
			return false;
		}

		std::unique_ptr<tdsb_logintoken_state_t> login_token(new tdsb_logintoken_state_t{});

		{
			int16_t login_status;
			int32_t resp_size;

			char buf[128];

			snprintf(buf, 128, "grant_type=password&username=%s&password=%s", 
				config::manager.get_value(config::TDSB_USER),
				config::manager.get_value(config::TDSB_PASS)
			);

			const char * headers[][2] = {
				{"X-Client-App-Info", "msign|||False|0.0.2|False|2147483647|False)"},
				{"Content-Type", "application/x-www-form-urlencoded"}, 
				{nullptr, nullptr}
			};


			auto cb = dwhttp::download_with_callback(
				"_zappsmaprd.tdsb.on.ca", 
				"/token", 
				headers,
				"POST",
				buf,
				login_status,
				resp_size
			);

			// Was the login successfull?
			
			if (login_status != 200) {
				dwhttp::stop_download();
				if (login_status == 401) {
					ESP_LOGW(TAG, "invalid credentials for tdsb");
				}
				else {
					ESP_LOGE(TAG, "invalid response from server for login");
				}
				return false;
			}

			tdsb_logintoken_start(login_token.get());

			// Run the response through NMFU
			for (int i = 0; i < resp_size; ++i) {
				auto value = cb();
				if (value <= 0) {
					dwhttp::stop_download();
					goto exit_readloop;
				}
				switch (tdsb_logintoken_feed(value, false, login_token.get())) {
					case TDSB_LOGINTOKEN_FAIL:
						{
							dwhttp::stop_download();
							ESP_LOGE(TAG, "logintoken parser failed; probably invalid");
							return false;
						}
					case TDSB_LOGINTOKEN_OK:
						break;
					case TDSB_LOGINTOKEN_DONE:
						dwhttp::stop_download();
						goto exit_readloop;
				}
			}
	exit_readloop:
			if (!login_token->c.login_ok) {
				ESP_LOGW(TAG, "invalid login token");
				return false;
			}
		}

		ESP_LOGD(TAG, "got logintoken %s", login_token->c.access_token);

		// First, request the daynames 
		char url[90];
		tm timinfo_now, timinfo_tommorow;

		{
			time_t now;
			time(&now);
			localtime_r(&now, &timinfo_now);
			now += 60*60*24;
			localtime_r(&now, &timinfo_tommorow);

			snprintf(url, 90, "/api/TimeTable/GetDayNameDayCycle/%s/20202021/Regular/%02d%02d%04d/%02d%02d%04d", 
				config::manager.get_value(config::TDSB_SCHOOLID),
				timinfo_now.tm_mday,
				timinfo_now.tm_mon + 1,
				timinfo_now.tm_year + 1900,
				timinfo_tommorow.tm_mday,
				timinfo_tommorow.tm_mon + 1,
				timinfo_tommorow.tm_year + 1900
			);

		}

		const char * headers[][2] = {
			{"X-Client-App-Info", "msign|||False|0.0.2|False|2147483647|False)"},
			{"Authorization", login_token->c.access_token},
			{nullptr, nullptr}
		};

		ESP_LOGD(TAG, "looking up day info with url %s", url);

		slots::TimetableHeader hdr{};

		{
			json::JSONParser dp([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
				if (stack[0]->array) { // don't use is_array because it's at the root
					if (v.type != json::Value::STR) return;
					if (strlen(v.str_val) < 6 || v.str_val[5] == ')') {
						(stack[0]->index ? hdr.next_day : hdr.current_day) = 0;
					}
					else {
						(stack[0]->index ? hdr.next_day : hdr.current_day) = v.str_val[5] - '0';
					}
				}
			});

			int16_t status_code;
			auto cb = dwhttp::download_with_callback("_zappsmaprd.tdsb.on.ca", url, headers, status_code);

			if (status_code < 200 || status_code >= 300) {
				dwhttp::stop_download();
				ESP_LOGW(TAG, "Got status code %d", status_code);
				return false;
			}

			// Parse
			if (!dp.parse(std::move(cb))) {
				ESP_LOGW(TAG, "failed to parse daycycle");
				dwhttp::stop_download();
				return false;
			}

			dwhttp::stop_download();
		}

		ESP_LOGD(TAG, "got day %d, %d", hdr.current_day, hdr.next_day);
		
		// Determine layouts
		if (hdr.current_day) {
			snprintf(url, 90, "/api/TimeTable/GetTimeTable/Student/%s/%02d%02d%04d",
				config::manager.get_value(config::TDSB_SCHOOLID),
				timinfo_now.tm_mday,
				timinfo_now.tm_mon + 1,
				timinfo_now.tm_year + 1900
			);

			int16_t status_code;
			auto cb = dwhttp::download_with_callback("_zappsmaprd.tdsb.on.ca", url, headers, status_code);
			if (status_code != 200) {
				ESP_LOGE(TAG, "failed to get layout0");
			}
			else if (!check_layout_for(
				std::move(cb), 
				hdr.current_layout
			)) {
				ESP_LOGE(TAG, "failed to parse layout0");
			}

			// If there was content, load the actual classes

			if (hdr.current_layout != hdr.EMPTY) {
				json::JSONParser tp([&](json::PathNode **stack, uint8_t sptr, const json::Value &jv){
					// Check if we're in the StudentCourse section

					if (!(sptr == 4 && stack[1]->is_array() && !strcmp(stack[1]->name, "CourseTable") && !strcmp(stack[2]->name, "StudentCourse") && jv.type == jv.STR))
						return;

					if (stack[1]->index > 1) {
						ESP_LOGW(TAG, "more than two courses");
						return;
					}
					
					bool pm = stack[1]->index == 1 || hdr.current_layout == hdr.PM_ONLY;

					// Actually load data
					if (!strcmp(stack[3]->name, "ClassCode")) {
						serial::interface.update_slot(pm ? slots::TIMETABLE_PM_CODE : slots::TIMETABLE_AM_CODE, jv.str_val);
					}
					else if (!strcmp(stack[3]->name, "TeacherName")) {
						serial::interface.update_slot(pm ? slots::TIMETABLE_PM_TEACHER : slots::TIMETABLE_AM_TEACHER, jv.str_val);
					}
					else if (!strcmp(stack[3]->name, "RoomNo")) {
						// If the thing _isn't_ at home update the room no
						if (strcmp(jv.str_val, "*AT HOME")) {
							serial::interface.update_slot(pm ? slots::TIMETABLE_PM_ROOM : slots::TIMETABLE_AM_ROOM, jv.str_val);
						}
						// Otherwise, delete the slot
						else serial::interface.delete_slot(pm ? slots::TIMETABLE_PM_ROOM : slots::TIMETABLE_AM_ROOM);
					}
					else if (!strcmp(stack[3]->name, "ClassName")) {
						serial::interface.update_slot(pm ? slots::TIMETABLE_PM_NAME : slots::TIMETABLE_AM_NAME, jv.str_val);
					}
				});

				// Re-download and parse it.
				auto cb = dwhttp::download_with_callback("_zappsmaprd.tdsb.on.ca", url, headers, status_code);
				if (status_code != 200) {
					ESP_LOGE(TAG, "failed to get course data");
				}
				else if (!tp.parse(
					std::move(cb)
				)) {
					ESP_LOGE(TAG, "failed to parse course data");
				}
				else {
					// If all that was OK, make sure to remove out old data if the layout says so
					switch (hdr.current_layout) {
						default:
							break;
						case slots::TimetableHeader::AM_ONLY:
						case slots::TimetableHeader::AM_INCLASS_ONLY:
							for (uint16_t x = slots::TIMETABLE_PM_CODE; x <= slots::TIMETABLE_PM_TEACHER; ++x)
								serial::interface.delete_slot(x);
							break;
						case slots::TimetableHeader::PM_ONLY:
							for (uint16_t x = slots::TIMETABLE_AM_CODE; x <= slots::TIMETABLE_AM_TEACHER; ++x)
								serial::interface.delete_slot(x);
							break;
					}
				}
			}
		}
		else {
			// Otherwise, delete all the entries
			for (uint16_t x = slots::TIMETABLE_AM_CODE; x <= slots::TIMETABLE_PM_TEACHER; ++x)
				serial::interface.delete_slot(x);
		}
		if (hdr.next_day) {
			snprintf(url, 90, "/api/TimeTable/GetTimeTable/Student/%s/%02d%02d%04d",
				config::manager.get_value(config::TDSB_SCHOOLID),
				timinfo_tommorow.tm_mday,
				timinfo_tommorow.tm_mon + 1,
				timinfo_tommorow.tm_year + 1900
			);

			int16_t status_code;
			auto cb = dwhttp::download_with_callback("_zappsmaprd.tdsb.on.ca", url, headers, status_code);
			if (status_code != 200) {
				ESP_LOGE(TAG, "failed to get layout1");
			}
			else if (!check_layout_for(
				std::move(cb), 
				hdr.next_layout
			)) {
				ESP_LOGE(TAG, "failed to parse layout1");
			}
		}
		// Stop any outstanding downloads
		dwhttp::stop_download();

		// Update the header
		serial::interface.update_slot(slots::TIMETABLE_HEADER, hdr);

		return true;
	}

}
