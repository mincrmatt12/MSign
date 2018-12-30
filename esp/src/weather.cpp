#include "weather.h"
#include "config.h"
#include "serial.h"
#include "util.h"
#include "json.h"
#include "time.h"
#include "wifi.h"
#include "TimeLib.h"

slots::WeatherInfo weather::info;
char * weather::info_buffer;
uint8_t weather::buffer_size;
uint8_t weather::echo_index = 0;

char weather::icon[16];

uint64_t weather::time_since_last_update = 0;

void weather::init() {
	weather::info_buffer = (char*)malloc(0);
	weather::buffer_size = 0;
	weather::echo_index = 0;

	memset(&weather::info, 0, sizeof(weather::info));
	memset(weather::icon, 0, 16);
}

void weather::loop() {
	if (time_since_last_update == 0 || (now() - time_since_last_update) > 240000) {
		// Do a weather update.
		
		if (!wifi::available()) return;
		if (config::manager.get_value(config::WEATHER_KEY) == nullptr) return;

		char url[128];
		snprintf(url, 128, "/forecast/%s/%s,%s?exclude=daily,hourly,alerts&units=ca", 
				config::manager.get_value(config::WEATHER_KEY),
				config::manager.get_value(config::WEATHER_LAT),
				config::manager.get_value(config::WEATHER_LONG));

		Serial1.println(url);

		util::Download down = util::download_from("darksky.i.mm12.xyz", url);

		if (down.error || down.buf == nullptr || down.length == 0) {
			if (down.buf != nullptr) free(down.buf);
			return;
		}

		json::JSONParser json([](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v) {
				// First, check if we're in the currently block.
				
				if (stack_ptr == 3 && strcmp(stack[1]->name, "currently") == 0) {
					// Alright, we need to pull out:
					//
					// Current temp
					// Icon (special: if it's partly-cloudy, just mark that)
					
					if (strcmp(stack[2]->name, "apparentTemperature") == 0 && v.type == json::Value::FLOAT) {
						// Store the apparent temperature
						weather::info.ctemp = v.float_val;
						Serial1.printf("temp = %f\n", v.float_val);
					}
					else if (strcmp(stack[2]->name, "icon") == 0 && v.type == json::Value::STR) {
						memset(weather::icon, 0, 16);
						if (strncmp("partly-cloudy", v.str_val, 13) == 0) {
							strcpy(weather::icon, "partly-cloudy");
						}
						else if (strlen(v.str_val) < 16) {
							strcpy(weather::icon, v.str_val);
						}

						Serial1.printf("wicon = %s\n", weather::icon);
					}
				}
		});
		
		json.parse(down.buf, down.length);

		time_since_last_update = now();
		free(down.buf);
	}
}
