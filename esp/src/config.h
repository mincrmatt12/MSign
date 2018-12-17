#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

namespace config {
	enum Entry : uint8_t {
		SSID,
		PSK,
		TIME_ZONE_SERVER,

		// leave me
		ENTRY_COUNT
	};

	extern const char * entry_names[ENTRY_COUNT];
	struct ConfigManager {
		ConfigManager();
		
		void load_from_sd();
		void check_for_new();
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
