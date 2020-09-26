#include <stdlib.h>
#include "config.h"
#include "util.h"
#include <esp_log.h>

#include "sd.h"

const static char* TAG = "config";

config::ConfigManager config::manager;

const char * config::entry_names[] = {
	"ssid",
	"psk",
	"ntpserver",
	"timezone",

	// TTC PARAMETERS
	"stopid1",
	"stopid2",
	"stopid3",
	"dirtag1",
	"dirtag2",
	"dirtag3",
	"shortname1",
	"shortname2",
	"shortname3",
	"alertsrch",

	// WEATHER PARAMETERS
	"wapikey",
	"wapilat",
	"wapilon",

	// CONFIG PARAMETERS
	"configuser",
	"configpass",
	
	// SCCFG PARAMETERS
	"scttc",
	"scweather",
	"sc3d",
	"sctimes",
	"sccalfix",

	"modelnames",
	"modelfocuses",
	"modelminposes",
	"modelmaxposes",
	"modelenable",
};


config::ConfigManager::ConfigManager() {
	this->data = (char *)malloc(128);
	memset(this->offsets, 0xFF, sizeof(this->offsets));
}

bool config::ConfigManager::reload_config() {
	free(this->data);
	this->data = (char *)malloc(128);
	this->size = 128;
	this->ptr = 0;
	memset(this->offsets, 0xFF, sizeof(this->offsets));

	return load_from_sd();
}

const char * config::ConfigManager::get_value(config::Entry e, const char * value) {
	if (this->offsets[e] == 0xFFFF) {
		return value;
	}
	return (this->data + this->offsets[e]);
}

bool config::ConfigManager::load_from_sd() {
	FIL config;
	switch (f_open(&config, "0:/config.txt", FA_READ)) {
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

	// Begin parsing it.
	char entry_name[16];
	char entry_value[256];
	bool mode = false;
	uint8_t pos = 0;
	UINT br;
	
	while (!f_eof(&config)) {
		char c;
		f_read(&config, &c, 1, &br);
		if (mode) {
			if (c != '\n') {
				entry_value[pos++] = c;
			}
			else {
				entry_value[pos++] = 0;

				int e;
				for (e = 0; e < config::ENTRY_COUNT; ++e) {
					if (strcmp(entry_name, config::entry_names[e]) == 0) {
						ESP_LOGI(TAG, "Set %s (%02x) = %s", entry_name, e, entry_value);
						add_entry(static_cast<Entry>(e), entry_value);
						break;
					}
				}

				if (e == config::ENTRY_COUNT) {
					ESP_LOGW(TAG, "Invalid key %s, ignoring", entry_name);
				}

				mode = false;
				pos = 0;
			}
		}
		else {
			if (c == '\n') {
				pos = 0;
			}
			else if (c == '=') {
				entry_name[pos++] = 0;
				pos = 0;
				mode = true;
			}
			else {
				entry_name[pos++] = c;
			}
		}
	}

	f_close(&config);
	return true;
}

void config::ConfigManager::add_entry(Entry e, const char * value) {
	uint8_t length = strlen(value) + 1;
	if (this->ptr + length >= this->size) {
		this->size += length + 16;
		this->data = (char *)realloc(this->data, this->size);
	}

	this->offsets[e] = this->ptr;
	memcpy(this->data + this->ptr, value, length);
	this->ptr += length;
}
