#include "config.h"

#include <SPI.h>
#include <SdFat.h>

config::ConfigManager config::manager;
const char * config::entry_names[] = {
	"ssid",
	"psk",
	"ntpserver"
};


config::ConfigManager::ConfigManager() {
	this->data = (char *)malloc(128);
	memset(this->offsets, 0xFF, sizeof(this->offsets));
}

const char * config::ConfigManager::get_value(config::Entry e, const char * value) {
	if (this->offsets[e] == 0xFFFF) {
		return value;
	}
	return (this->data + this->offsets[e]);
}

void config::ConfigManager::check_for_new() {} // TODO

void config::ConfigManager::load_from_sd() {
	// Alright, here comes the stupidity
	
	SdFatSoftSpi<D6, D2, D5> sd;
	if (!sd.begin(D1)) {
		Serial1.println(F("SD Card couldn't init.\n"));
		delay(1000);
		ESP.restart();
	}
	
	// Make sure the config file exists before we read it.
	
	if (!sd.exists("config.txt")) {
		Serial1.println(F("Config file missing, seed it with ssid/psdk/url and make the last entry update=now, or place the entire config there"));
		delay(1000);
		ESP.restart();
	}

	// Open the file
	
	SdFile config("config.txt", O_READ);

	// Begin parsing it.
	char entry_name[16];
	char entry_value[256];
	bool mode = false;
	uint8_t pos = 0;
	
	while (config.available()) {
		char c = config.read();
		if (mode) {
			if (c != '\n') {
				entry_value[pos++] = c;
			}
			else {
				entry_value[pos++] = 0;

				for (int e = 0; e < config::ENTRY_COUNT; ++e) {
					if (strcmp(config::entry_names[e], entry_name) == 0) {
						add_entry(static_cast<Entry>(e), entry_value);
						Serial1.printf("Set %s (%02x) = %s\n", entry_name, e, entry_value);
					}
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

	config.close();

	// Hope the SD card gets freed
}

void config::ConfigManager::add_entry(Entry e, const char * value) {
	uint8_t length = strlen(value) + 1;
	if (this->ptr + length >= this->size) {
		this->size += length * 2;
		this->data = (char *)realloc(this->data, this->size);
	}

	this->offsets[e] = this->ptr;
	memcpy(this->data + this->ptr, value, length);
	this->ptr += length;
}
