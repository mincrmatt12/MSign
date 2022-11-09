#include "config.h"
#include <esp_log.h>
#include "sd.h"

const static char * const TAG = "cfgload";

namespace config {
	struct sd_load_source {
		// All of this junk is to emulate a shared_ptr since move_only_function is only C++23
		static int reentrant_lock;

		bool ok = false;
		FIL config{};

		sd_load_source(){};
		sd_load_source(bool ok, FIL config) : ok(ok), config(config) {
			if (ok) reentrant_lock++;
		};
		sd_load_source(const sd_load_source& other) {
			ok = other.ok;
			config = other.config;
			if (ok) ++reentrant_lock;
		}
		sd_load_source(sd_load_source&& other) {
			config = other.config;
			ok = std::exchange(other.ok, false);
		}
		~sd_load_source() {
			if (ok) {
				--reentrant_lock;
				ok = false;
				if (reentrant_lock) return;
				f_close(&config);
			}
		}

		sd_load_source& operator=(const sd_load_source& other) {
			if (ok) {
				--reentrant_lock;
				ok = false;
				if (!reentrant_lock) f_close(&config);
			}
			ok = other.ok;
			config = other.config;
			if (ok) ++reentrant_lock;
			return (*this);
		}
		sd_load_source& operator=(sd_load_source&& other) {
			config = other.config;
			ok = std::exchange(other.ok, false);
			return (*this);
		}

		int16_t operator()() {
			if (!ok || !reentrant_lock) return -1;
			uint8_t value;
			UINT br;
			return f_eof(&config) ? -1 : (f_read(&config, &value, 1, &br) == FR_OK && br == 1 ? value : -1);
		}
	};

	int sd_load_source::reentrant_lock = 0;

	json::TextCallback sd_cfg_load_source() {
		if (sd_load_source::reentrant_lock) return sd_load_source{};

		FIL config;
		switch (f_open(&config, "0:/config.json", FA_READ)) {
			case FR_OK:
				return sd_load_source{true, config};
			case FR_NO_FILE:
			case FR_NO_PATH:
				ESP_LOGE(TAG, "Could not find the configuration. Place it on the SD card at /config.txt and restart.");
				return sd_load_source{};
			default:
				ESP_LOGE(TAG, "Could not load the configuration, please check the SD");
				return sd_load_source{};
		}
	}

	bool parse_config_from_sd() {
		return parse_config(sd_cfg_load_source());
	}
}
