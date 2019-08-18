#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

namespace config {
	enum Entry : uint8_t {
		SSID,
		PSK,
		TIME_ZONE_SERVER,

		// TTC stuff
		STOPID1,
		STOPID2,
		STOPID3,
		DTAG1,
		DTAG2,
		DTAG3,
		SNAME1,
		SNAME2,
		SNAME3,

		WEATHER_KEY,
		WEATHER_LAT,
		WEATHER_LONG,

		CONFIG_USER,
		CONFIG_PASS,

		SC_TTC,
		SC_WEATHER,
		SC_3D,
		SC_TIMES,
		SC_CFIX,

		// leave me
		ENTRY_COUNT
	};

	extern const char * entry_names[ENTRY_COUNT];
	struct ConfigManager {
		ConfigManager();
		
		void load_from_sd();
		bool use_new_config(const char * data, uint32_t size);
		const char * get_value(Entry e, const char * value=nullptr);
	private:
		void add_entry(Entry e, const char *buffer);

		char *data;
		uint16_t offsets[ENTRY_COUNT] = {0xFFFF};
		uint16_t ptr = 0;
		uint16_t size = 128;
	};

	extern ConfigManager manager;
}

#endif
