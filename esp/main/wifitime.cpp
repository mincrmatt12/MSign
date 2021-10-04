#include "wifitime.h"
#include "common/slots.h"
#include "config.h"
#include "serial.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_wpa2.h>

#include "wifitime.cfg.h"

#include <lwip/apps/sntp.h>
#include "grabber/grab.h"

EventGroupHandle_t wifi::events;

static const char * TAG = "wifiman";
static const char * T_TAG = "sntp";

void sntp_timer_cb(TimerHandle_t xTimer) {
	// Wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { };

	if (!(xEventGroupGetBits(wifi::events) & wifi::WifiConnected)) goto exit;

	time(&now);
	localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGI(T_TAG, "Still waiting for time..");
		return;
    }

	ESP_LOGI(T_TAG, "Time is %s", asctime(&timeinfo));
	xEventGroupSetBits(wifi::events, wifi::TimeSynced);
	ESP_LOGI(T_TAG, "Free heap after connect %d", esp_get_free_heap_size());
	// Stop this timer
exit:
	xTimerDelete(xTimer, pdMS_TO_TICKS(100));
}

void kill_grab_task(TimerHandle_t xTimer) {
	ESP_LOGW(TAG, "killing grabber task");
	xEventGroupSetBits(wifi::events, wifi::GrabTaskStop);
}

void start_grab_task(TimerHandle_t xTimer) {
	static bool die = false;

	if (die || xTaskCreate(grabber::run, "grab", 6240, nullptr, 6, NULL) == pdPASS) {
		if (!xTimerStop(xTimer, pdMS_TO_TICKS(5))) {
			die = true;
		}
		else die = false;
	}
	else if (!die) {
		ESP_LOGW(TAG, "failed to start grabber again...");
	}
}

TimerHandle_t grab_killer = nullptr;
TimerHandle_t grab_starter = nullptr;
bool grab_never_started = true;

esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
    system_event_info_t &info = event->event_info;
	slots::WifiStatus newstatus;

	switch (event->event_id) {
		case SYSTEM_EVENT_STA_START:
			tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "msign");
			// Send disconnected message
			newstatus.connected = false;
			serial::interface.update_slot(slots::WIFI_STATUS, newstatus);
			esp_wifi_connect();
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG, "Connected with IP %s", ip4addr_ntoa(&info.got_ip.ip_info.ip));
			newstatus.connected = true;
			for (int i = 0; i < 4; ++i) {
				newstatus.ipaddr[i] = ip4_addr_get_byte_val(info.got_ip.ip_info.ip, i);
				newstatus.gateway[i] = ip4_addr_get_byte_val(info.got_ip.ip_info.gw, i);
			}
			serial::interface.update_slot(slots::WIFI_STATUS, newstatus);
			xEventGroupSetBits(wifi::events, wifi::WifiConnected);

			if (grab_killer) {
				ESP_LOGD(TAG, "disabling grab killer");
				xTimerDelete(grab_killer, pdMS_TO_TICKS(100));
				grab_killer = nullptr;
			}

			if ((grab_never_started || (xEventGroupGetBits(wifi::events) & wifi::GrabTaskDead)) && xTaskCreate(grabber::run, "grab", 6240, nullptr, 6, NULL) != pdPASS) {
				ESP_LOGE(TAG, "Failed to create grabber task! scheduling for later");
				if (grab_starter == nullptr) grab_starter = xTimerCreate("grs", pdMS_TO_TICKS(500), true, nullptr, start_grab_task);
				xTimerReset(grab_starter, pdMS_TO_TICKS(100));
			}
			else {
				if (grab_never_started) grab_never_started = false;
				ESP_LOGI(TAG, "started grab task!");
				xEventGroupClearBits(wifi::events, wifi::GrabTaskStop);
			}

			// Check if time is set
			if (xEventGroupGetBits(wifi::events) & wifi::TimeSynced) {
				break;
			}
			
			// Otherwise, start the sntp server.
			ESP_LOGI(T_TAG, "Setting up SNTP");

			sntp_setoperatingmode(SNTP_OPMODE_POLL);
			// This is ugly, but I'm _fairly_ sure lwip doesn't screw with this [citation needed]; plus the examples pass in a constant here so /shrug
			sntp_setservername(0, const_cast<char *>(wifi::time_server));
			sntp_init();
			
			// Set the timezone
			ESP_LOGI(T_TAG, "Timezone is %s", wifi::time_zone_str);
			setenv("TZ", wifi::time_zone_str, 1);
			tzset();

			// Start a software timer to check for sntp
			{
				auto tmr = xTimerCreate("snwa", pdMS_TO_TICKS(2000), true, NULL, sntp_timer_cb);
				if (tmr) xTimerStart(tmr, pdMS_TO_TICKS(2000));
				else ESP_LOGW(TAG, "out of timers!");
			}
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			ESP_LOGW(TAG, "Disconnected from AP (%d)", info.disconnected.reason);
			newstatus.connected = false;
			serial::interface.update_slot(slots::WIFI_STATUS, newstatus);
			xEventGroupClearBits(wifi::events, wifi::WifiConnected);
			if (info.disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
				esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
			}
			else if (grab_killer == nullptr) {
				grab_killer = xTimerCreate("grk", pdMS_TO_TICKS(4000), false, nullptr, kill_grab_task);
				xTimerStart(grab_killer, pdMS_TO_TICKS(5));
				ESP_LOGD(TAG, "scheduled grab killer");
			}
			ESP_LOGW(TAG, "Trying to reconnect...");
			esp_wifi_connect();
			break;
		default:
			break;
	}
	return ESP_OK;
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

	struct wifi_blob {
		wifi_config_t wifi_config_data{};
		wifi_country_t country{};
		bool enterprise_wifi_enable = false;
	} *wifi_cfg_blob;
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

void wifi::receive_config(const char * field, const char * value) {
	if (!wifi_cfg_blob) {
		wifi_cfg_blob = new wifi_blob{};
	}

	auto wifi_config_data = &wifi_cfg_blob->wifi_config_data;

	if (strcmp(field, "ssid") == 0) {
		strncpy((char *)wifi_config_data->sta.ssid, value, 32);
	}
	else if (strcmp(field, "psk") == 0) {
		strncpy((char *)wifi_config_data->sta.password, value, 64);
		wifi_config_data->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
		esp_wifi_sta_wpa2_ent_disable();
	}
	else if (strcmp(field, "username") == 0) {
		wifi_cfg_blob->enterprise_wifi_enable = true;
		esp_wifi_sta_wpa2_ent_set_username((const uint8_t *)value, strlen(value));
		esp_wifi_sta_wpa2_ent_set_identity((const uint8_t *)value, strlen(value));
	}
	else if (strcmp(field, "username_only") == 0) {
		wifi_cfg_blob->enterprise_wifi_enable = true;
		esp_wifi_sta_wpa2_ent_set_username((const uint8_t *)value, strlen(value));
	}
	else if (strcmp(field, "identity") == 0) {
		wifi_cfg_blob->enterprise_wifi_enable = true;
		esp_wifi_sta_wpa2_ent_set_identity((const uint8_t *)value, strlen(value));
	}
	else if (strcmp(field, "password") == 0) {
		wifi_cfg_blob->enterprise_wifi_enable = true;
		esp_wifi_sta_wpa2_ent_set_password((const uint8_t *)value, strlen(value));
	}
	else if (strcmp(field, "channel") == 0) {
		wifi_config_data->sta.channel = atoi(value);
	}
	else if (strcmp(field, "bssid") == 0) {
		int a, b, c, d, e, f;
		sscanf(value, "%x:%x:%x:%x:%x:%x", &a, &b, &c, &d, &e, &f);
		wifi_config_data->sta.bssid_set = true;
		wifi_config_data->sta.bssid[0] = a;
		wifi_config_data->sta.bssid[1] = b;
		wifi_config_data->sta.bssid[2] = c;
		wifi_config_data->sta.bssid[3] = d;
		wifi_config_data->sta.bssid[4] = e;
		wifi_config_data->sta.bssid[5] = f;
	}
	else if (strcmp(field, "country_settings") == 0) {

		int schan, fchan, maxtx;

		sscanf(value, "%2s:%d:%d:%d", wifi_cfg_blob->country.cc, &schan, &fchan, &maxtx);
		wifi_cfg_blob->country.schan = schan;
		wifi_cfg_blob->country.nchan = fchan - schan + 1;
		wifi_cfg_blob->country.max_tx_power = maxtx * 4;
		wifi_cfg_blob->country.policy = WIFI_COUNTRY_POLICY_MANUAL;
	}
	else {
		ESP_LOGW(TAG, "unknown wifi config param %s", field);
	}
}

bool wifi::init() {
	// Verify we have ssid/psk
	xEventGroupSetBits(wifi::events, wifi::GrabTaskDead);
	
	if (!wifi_cfg_blob) {
		ESP_LOGE(TAG, "No wifi SSID is set, halting.");
		return false;
	}

	// Setup tcpip/event handlers
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, nullptr));

	// Initialize wifi -- performed here so config can call wifi funcs
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	// Setup with config data
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg_blob->wifi_config_data));
	if (wifi_cfg_blob->enterprise_wifi_enable) esp_wifi_sta_wpa2_ent_enable();
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_country(&wifi_cfg_blob->country));
	delete wifi_cfg_blob; wifi_cfg_blob = nullptr;

	return true;
}
