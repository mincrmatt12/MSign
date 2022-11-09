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

static const char * const TAG = "wifiman";
static const char * const T_TAG = "sntp";

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

void start_grab_task() {
	xEventGroupClearBits(wifi::events, wifi::GrabTaskStop);
}

TimerHandle_t grab_killer = nullptr;
bool grab_never_started = true;

void kill_grab_task(TimerHandle_t xTimer) {
	if (grab_killer == nullptr) return;
	ESP_LOGW(TAG, "killing grabber task");
	xEventGroupSetBits(wifi::events, wifi::GrabTaskStop);
}

slots::WifiStatus current_wifi_status;

esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
    system_event_info_t &info = event->event_info;

	switch (event->event_id) {
		case SYSTEM_EVENT_STA_START:
			// Send disconnected message
			current_wifi_status.connected = false;
			serial::interface.update_slot_nosync(slots::WIFI_STATUS, current_wifi_status);
			break;
		case SYSTEM_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG, "Connected with IP %s", ip4addr_ntoa(&info.got_ip.ip_info.ip));
			current_wifi_status.connected = true;
			for (int i = 0; i < 4; ++i) {
				current_wifi_status.ipaddr[i] = ip4_addr_get_byte_val(info.got_ip.ip_info.ip, i);
				current_wifi_status.gateway[i] = ip4_addr_get_byte_val(info.got_ip.ip_info.gw, i);
			}
			serial::interface.update_slot_nosync(slots::WIFI_STATUS, current_wifi_status);
			xEventGroupSetBits(wifi::events, wifi::WifiConnected);

			if (grab_killer) {
				ESP_LOGD(TAG, "disabling grab killer");
				xTimerDelete(grab_killer, pdMS_TO_TICKS(100));
				grab_killer = nullptr;
			}

			start_grab_task();

			// Check if time is set
			if (xEventGroupGetBits(wifi::events) & wifi::TimeSynced) {
				break;
			}
			
			// Otherwise, start the sntp server.
			ESP_LOGI(T_TAG, "Setting up SNTP");

			sntp_setoperatingmode(SNTP_OPMODE_POLL);
			// This is ugly, but I'm _fairly_ sure lwip doesn't screw with this [citation needed]; plus the examples pass in a constant here so /shrug
			sntp_setservername(0, const_cast<char *>(wifi::time_server.get()));
			sntp_init();
			
			// Set the timezone
			ESP_LOGI(T_TAG, "Timezone is %s", wifi::time_zone_str.get());
			setenv("TZ", wifi::time_zone_str, 1);
			tzset();

			// Start a software timer to check for sntp
			{
				auto tmr = xTimerCreate("snwa", pdMS_TO_TICKS(2000), true, NULL, sntp_timer_cb);
				if (tmr) xTimerStart(tmr, pdMS_TO_TICKS(2000));
				else ESP_LOGW(TAG, "out of timers!");
			}
			break;
		case SYSTEM_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "Associated with AP");
			break;
		case SYSTEM_EVENT_STA_DISCONNECTED:
			ESP_LOGW(TAG, "Disconnected from AP (%d)", info.disconnected.reason);
			current_wifi_status.connected = false;
			serial::interface.update_slot_nosync(slots::WIFI_STATUS, current_wifi_status);
			xEventGroupClearBits(wifi::events, wifi::WifiConnected);
			if (info.disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
				esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
			}
			else if (grab_killer == nullptr) {
				grab_killer = xTimerCreate("grk", pdMS_TO_TICKS(7000), false, nullptr, kill_grab_task);
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



// stolen from stackoverflow
static int days_from_civil(int y, int m, int d)
{
	y -= m <= 2;
	int era = y / 400;
	int yoe = y - era * 400;                                   // [0, 399]
	int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
	int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
	return era * 146097 + doe - 719468;
}

time_t wifi::timegm(tm const* t)   
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
	xEventGroupSetBits(wifi::events, wifi::GrabTaskStop);
	WifiSdConfig cfg_blob{};

	if (!load_wifi_config(cfg_blob)) {
		ESP_LOGE(TAG, "Failed to load wifi cfg, halting.");
		return false;
	}

	// Verify we have ssid/psk
	if (!cfg_blob.ssid) {
		ESP_LOGE(TAG, "No wifi SSID is set, halting.");
		return false;
	}

	// Setup tcpip/event handlers
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, nullptr));

	// Initialize wifi
	{
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	}

	// Setup with config data
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	{
		wifi_config_t cfg{};

		strncpy((char *)cfg.sta.ssid, cfg_blob.ssid, 32);
		ESP_LOGI(TAG, "Connecting to SSID %s", cfg.sta.ssid);

		if (cfg_blob.psk) {
			strncpy((char *)cfg.sta.password, cfg_blob.psk, 64);
			cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
			ESP_LOGI(TAG, "... using a PSK");
		}
		else if (cfg_blob.enterprise) {
			//cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_ENTERPRISE;
			ESP_LOGI(TAG, "... using WPA2-enterprise");
		}
		else {
			cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
		}

		if ((cfg.sta.bssid_set = cfg_blob.bssid_set)) {
			ESP_LOGI(TAG, "... using a specific BSSID");
		}
		memcpy(cfg.sta.bssid, cfg_blob.bssid, 6);

		if ((cfg.sta.channel = cfg_blob.channel)) {
			ESP_LOGI(TAG, "... using a specific channel");
		}
	
		ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg));
		if (cfg_blob.enterprise) {
			// setup enterprise cfg
			esp_wifi_sta_wpa2_ent_set_username((const uint8_t *)cfg_blob.enterprise.username(), strlen(cfg_blob.enterprise.username()));
			ESP_LOGI(TAG, "... using username %s", cfg_blob.enterprise.username());
			esp_wifi_sta_wpa2_ent_set_identity((const uint8_t *)cfg_blob.enterprise.identity(), strlen(cfg_blob.enterprise.identity()));
			esp_wifi_sta_wpa2_ent_set_password((const uint8_t *)cfg_blob.enterprise.password(), strlen(cfg_blob.enterprise.password()));

			esp_wifi_sta_wpa2_ent_enable();
		}
	}
	ESP_ERROR_CHECK(esp_wifi_start());
	
	if (cfg_blob.country) {
		wifi_country_t country_settings{};
		country_settings.policy = WIFI_COUNTRY_POLICY_MANUAL;
		strncpy(country_settings.cc, cfg_blob.country.code, 3);
		country_settings.max_tx_power = cfg_blob.country.max_power_db * 4;
		country_settings.schan = cfg_blob.country.schan;
		country_settings.nchan = cfg_blob.country.nchan;
		ESP_LOGI(TAG, "... with country %s (channel %d - %d, power %d dB)", country_settings.cc, country_settings.schan, country_settings.schan + country_settings.nchan - 1, cfg_blob.country.max_power_db);

		ESP_ERROR_CHECK(esp_wifi_set_country(&country_settings));
	}

	tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, cfg_blob.hostname);

	ESP_ERROR_CHECK(esp_wifi_connect());

	return true;
}
