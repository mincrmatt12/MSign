#include "config.h"
#include <SPI.h>
#include <SdFat.h>
#include "sys/pgmspace.h"
#include "util.h"

extern SdFatSoftSpi<D6, D2, D5> sd;

namespace config { namespace entry_name_s {
	const char s_ssid[] PROGMEM = "ssid";
	const char s_psk[] PROGMEM = "psk";
	const char s_ntpserver[] PROGMEM = "ntpserver";

	// TTC PARAMETERS
	const char s_stopid1[] PROGMEM = "stopid1";
	const char s_stopid2[] PROGMEM = "stopid2";
	const char s_stopid3[] PROGMEM = "stopid3";
	const char s_dirtag1[] PROGMEM = "dirtag1";
	const char s_dirtag2[] PROGMEM = "dirtag2";
	const char s_dirtag3[] PROGMEM = "dirtag3";
	const char s_shortname1[] PROGMEM = "shortname1";
	const char s_shortname2[] PROGMEM = "shortname2";
	const char s_shortname3[] PROGMEM = "shortname3";

	// WEATHER PARAMETERS
	const char s_wapikey[] PROGMEM = "wapikey";
	const char s_wapilat[] PROGMEM = "wapilat";
	const char s_wapilon[] PROGMEM = "wapilon";

	// CONFIG PARAMETERS
	const char s_configuser[] PROGMEM = "configuser";
	const char s_configpass[] PROGMEM = "configpass";
		
	// SCCFG PARAMETERS
	const char s_scttc[] PROGMEM = "scttc";
	const char s_scweather[] PROGMEM = "scweather";
	const char s_sc3d[] PROGMEM = "sc3d";
	const char s_sctimes[] PROGMEM = "sctimes";
	const char s_sccalfix[] PROGMEM = "sccalfix";

	const char s_modelnames[] PROGMEM = "modelnames";
	const char s_modelfocuses[] PROGMEM = "modelfocuses";
	const char s_modelminposes[] PROGMEM = "modelminposes";
	const char s_modelmaxposes[] PROGMEM = "modelmaxposes";
	const char s_modelenable[] PROGMEM = "modelenable";
} }

config::ConfigManager config::manager;
const char * config::entry_names[] = {
	config::entry_name_s::s_ssid,
	config::entry_name_s::s_psk,
	config::entry_name_s::s_ntpserver,

	// TTC PARAMETERS
	config::entry_name_s::s_stopid1,
	config::entry_name_s::s_stopid2,
	config::entry_name_s::s_stopid3,
	config::entry_name_s::s_dirtag1,
	config::entry_name_s::s_dirtag2,
	config::entry_name_s::s_dirtag3,
	config::entry_name_s::s_shortname1,
	config::entry_name_s::s_shortname2,
	config::entry_name_s::s_shortname3,

	// WEATHER PARAMETERS
	config::entry_name_s::s_wapikey,
	config::entry_name_s::s_wapilat,
	config::entry_name_s::s_wapilon,

	// CONFIG PARAMETERS
	config::entry_name_s::s_configuser,
	config::entry_name_s::s_configpass,
	
	// SCCFG PARAMETERS
	config::entry_name_s::s_scttc,
	config::entry_name_s::s_scweather,
	config::entry_name_s::s_sc3d,
	config::entry_name_s::s_sctimes,
	config::entry_name_s::s_sccalfix,

	config::entry_name_s::s_modelnames,
	config::entry_name_s::s_modelfocuses,
	config::entry_name_s::s_modelminposes,
	config::entry_name_s::s_modelmaxposes,
	config::entry_name_s::s_modelenable,
};


config::ConfigManager::ConfigManager() {
	this->data = (char *)malloc(128);
	memset(this->offsets, 0xFF, sizeof(this->offsets));
}

bool config::ConfigManager::use_new_config(const char * data, uint32_t size) {
	SdFile config("config.txt", O_WRITE);
	config.truncate(0); // erase file.
	config.write(data, size);
	config.close();

	free(this->data);
	this->data = (char *)malloc(128);
	this->size = 128;
	this->ptr = 0;
	memset(this->offsets, 0xFF, sizeof(this->offsets));

	load_from_sd();

	return true;
}

const char * config::ConfigManager::get_value(config::Entry e, const char * value) {
	if (this->offsets[e] == 0xFFFF) {
		return value;
	}
	return (this->data + this->offsets[e]);
}

void config::ConfigManager::load_from_sd() {
	// Make sure the config file exists before we read it.
	
	sd.chdir();
	if (!sd.exists("config.txt")) {
		Log.println(F("Config file missing, seed it with ssid/psdk/url and make the last entry update=now, or place the entire config there"));
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

				int e;
				for (e = 0; e < config::ENTRY_COUNT; ++e) {
					if (strcmp_P(entry_name, config::entry_names[e]) == 0) {
						add_entry(static_cast<Entry>(e), entry_value);
						Log.printf_P(PSTR("Set %s (%02x) = %s\n"), entry_name, e, entry_value);
						break;
					}
				}

				if (e == config::ENTRY_COUNT) Log.printf_P(PSTR("Invalid key %s\n"), entry_name);

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
