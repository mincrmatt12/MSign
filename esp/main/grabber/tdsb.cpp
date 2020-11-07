#include "tdsb.h"
#include "../dwhttp.h"
#include "../config.h"
#include "../json.h"
#include <esp_log.h>
#include <memory>
#include <ctime>
#include "../common/slots.h"

extern "C" {
#include <tdsb_logintoken.h>
};

const static char* TAG = "tdsb";

void tdsb::init() {
}

bool tdsb::loop() {
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
	char url[80];

	{
		time_t now;
		time(&now);
		tm timinfo_now, timinfo_tommorow;
		localtime_r(&now, &timinfo_now);
		now += 60*60*24;
		localtime_r(&now, &timinfo_tommorow);

		snprintf(url, 80, "/api/TimeTable/GetDayNameDayCycle/%s/20202021/Regular/%02d%02d%04d/%02d%02d%04d", 
			config::manager.get_value(config::TDSB_SCHOOLID),
			timinfo_now.tm_mday,
			timinfo_now.tm_mon + 1,
			timinfo_now.tm_year + 1900,
			timinfo_tommorow.tm_mday,
			timinfo_tommorow.tm_mon + 1,
			timinfo_tommorow.tm_year + 1900
		);

	}

	ESP_LOGD(TAG, "looking up day info with url %s", url);

	slots::TimetableHeader hdr;

	{
		json::JSONParser dp([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v){
			if (stack[1]->is_array()) {
				if (v.type != json::Value::STR) return;
				if (strlen(v.str_val) < 6 || v.str_val[5] == ')') {
					(stack[1]->index ? hdr.next_day : hdr.current_day) = 0;
				}
				else {
					(stack[1]->index ? hdr.next_day : hdr.current_day) = v.str_val[5] - '0';
				}
			}
		});

		const char * headers[][2] = {
			{"X-Client-App-Info", "msign|||False|0.0.2|False|2147483647|False)"},
			{"Authorization", login_token->c.access_token},
			{nullptr, nullptr}
		};

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

	return true;
}
