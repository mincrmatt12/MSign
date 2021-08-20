#include "wifitime.h"
#include "serial.h"
#include "wifitime.cfg.h"

#include <esp_log.h>
#include <sys/time.h>

EventGroupHandle_t wifi::events;

static const char * TAG = "wifiman";
static const char * T_TAG = "sntp";

namespace {
	// stolen from stackoverflow
	int days_from_civil(int y, int m, int d)
	{
		y -= m <= 2;
		int era = y / 400;
		int yoe = y - era * 400;                                   // [0, 399]
		int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
		int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
		return era * 146097 + doe - 719468;
	}

	time_t timegm(tm const* t)   
	{
		int year = t->tm_year + 1900;
		int month = t->tm_mon;  
		if (month > 11)
		{
			year += month / 12;
			month %= 12;
		}
		else if (month < 0)
		{
			int years_diff = (11 - month) / 12;
			year -= years_diff;
			month += 12 * years_diff;
		}
		int days_since_1970 = days_from_civil(year, month + 1, t->tm_mday);

		return 60 * (60 * (24L * days_since_1970 + t->tm_hour) + t->tm_min) + t->tm_sec;
	}
}

void wifi::receive_config(const char* k, const char *v) {
	ESP_LOGI(TAG, "got %s == %s", k, v);
}

uint64_t wifi::get_localtime() {
	time_t now;
	struct tm current_time;
	struct timeval ts;
	gettimeofday(&ts, NULL);
	time(&now);
	localtime_r(&now, &current_time);
	now = timegm(&current_time);
	uint64_t millis = ((uint64_t)now * 1000) + ts.tv_usec / 1000;
	return millis;
}

uint64_t wifi::millis_to_local(uint64_t millis) {
	time_t now = millis / 1000;
	struct tm current_time;
	localtime_r(&now, &current_time);
	now = timegm(&current_time);
	return ((uint64_t)now * 1000) + millis % 1000;
}

bool wifi::init() {
	// Init events
	events = xEventGroupCreate();

	ESP_LOGW(TAG, "Fake wifi init: ok");

	slots::WifiStatus fakestatus;
	fakestatus.connected = true;
	fakestatus.ipaddr[0] = 192;
	fakestatus.ipaddr[1] = 168;
	fakestatus.ipaddr[2] = 122;
	fakestatus.ipaddr[3] = 2;

	fakestatus.gateway[0] = 192;
	fakestatus.gateway[1] = 168;
	fakestatus.gateway[2] = 122;
	fakestatus.gateway[3] = 1;

	serial::interface.update_slot(slots::WIFI_STATUS, fakestatus);

	xEventGroupSetBits(events, wifi::WifiConnected|wifi::TimeSynced);

	return true;
}
