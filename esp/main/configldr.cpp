#include "config.h"
#include <esp_log.h>
#include "sd.h"

const static char * TAG = "cfgload";

namespace config {
	bool parse_config_from_sd() {
		FIL config;
		switch (f_open(&config, "0:/config.json", FA_READ)) {
			case FR_OK:
				break;
			case FR_NO_FILE:
			case FR_NO_PATH:
				ESP_LOGE(TAG, "Could not find the configuration. Place it on the SD card at /config.txt and restart.");
				return false;
			default:
				ESP_LOGE(TAG, "Could not load the configuration, please check the SD");
				return false;
		}

		bool cfg_ok = parse_config([&](){
			uint8_t value;
			UINT br;
			return f_eof(&config) ? -1 : (f_read(&config, &value, 1, &br) == FR_OK && br == 1 ? value : -1);
		});

		f_close(&config);

		return cfg_ok;
	}
}
