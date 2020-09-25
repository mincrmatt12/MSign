#include "wifitime.h"
#include "util.h"
#include "common/slots.h"
#include "config.h"
#include "serial.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>

#include <lwip/apps/sntp.h>

EventGroupHandle_t wifi::events;

static const char * TAG = "wifiman";
static const char * T_TAG = "sntp";

esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
    system_event_info_t &info = event->event_info;

	switch (event->event_id) {
		case SYSTEM_EVENT_STA_START:
			tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "msign");
			esp_wifi_connect();
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG, "Connected with IP %s", ip4addr_ntoa(&info.got_ip.ip_info.ip));
			xEventGroupSetBits(wifi::events, wifi::WifiConnected);
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			ESP_LOGW(TAG, "Disconnected from AP (%d)", info.disconnected.reason);
			if (info.disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
				esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
			}
			ESP_LOGW(TAG, "Trying to reconnect...");
			esp_wifi_connect();
			xEventGroupClearBits(wifi::events, wifi::WifiConnected);
			break;
		default:
			break;
	}
	return ESP_OK;
}

void sntp_task(void *) {
	// This task deletes itself once the time is set.
	xEventGroupWaitBits(wifi::events, wifi::WifiConnected, false, true, portMAX_DELAY);

	// Setup SNTP
	ESP_LOGI(T_TAG, "Setting up SNTP");

	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	// This is ugly, but I'm _fairly_ sure lwip doesn't screw with this [citation needed]; plus the examples pass in a constant here so /shrug
	sntp_setservername(0, const_cast<char *>(config::manager.get_value(config::TIME_ZONE_SERVER, "pool.ntp.org")));
	sntp_init();
	
	// Set the timezone
	auto tz = config::manager.get_value(config::TIME_ZONE_STR, "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00");
	ESP_LOGI(T_TAG, "Timezone is %s", tz);
	setenv("TZ", tz, 1);
	tzset();

	// Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { };
    int retry = 0;

    while (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGI(T_TAG, "Still waiting for time (%d/)", retry);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

	ESP_LOGI(T_TAG, "Time is %s", asctime(&timeinfo));
	xEventGroupSetBits(wifi::events, wifi::TimeSynced);
	ESP_LOGI(T_TAG, "Free heap after connect %d", esp_get_free_heap_size());
	vTaskDelete(NULL);
}

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
	// Verify we have ssid/psk
	
	const char *ssid = config::manager.get_value(config::SSID), *psk = config::manager.get_value(config::PSK);
	if (!ssid) {
		ESP_LOGE(TAG, "No wifi SSID is set, halting.");
		return false;
	}

	// Init events
	events = xEventGroupCreate();

	// Setup tcpip/event handlers
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, nullptr));

	// Initialize wifi
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	// Setup with config data
	wifi_config_t w_cfg = {};
	strncpy((char *)w_cfg.sta.ssid, ssid, 32);
	if (psk) {
		strncpy((char *)w_cfg.sta.password, psk, 32);
		w_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &w_cfg));
	ESP_ERROR_CHECK(esp_wifi_start());

	// Start the time wait service
	xTaskCreate(sntp_task, "tWAIT", 2048, NULL, 7, NULL);

	return true;
}
