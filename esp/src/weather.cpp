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

slots::VStr vsw;

void weather::init() {
	weather::info_buffer = nullptr;
	weather::buffer_size = 0;
	weather::echo_index = 0;

	memset(&weather::info, 0, sizeof(weather::info));
	memset(weather::icon, 0, 16);

	serial::interface.register_handler([](uint16_t data_id, uint8_t * buffer, uint8_t & length){
		if (data_id == slots::WEATHER_STATUS) {
			// VStr
			vsw.index = echo_index;
			vsw.size = buffer_size;
			if (buffer_size - echo_index > 14) {
				memcpy(vsw.data, (weather::info_buffer + vsw.index), 14);
				echo_index += 14;
			}
			else {
				memcpy(vsw.data, (weather::info_buffer + vsw.index), (buffer_size-echo_index));
				echo_index = 0;
			}

			memcpy(buffer, &vsw, sizeof(vsw));
			length = sizeof(vsw);
			return true;
		}
		return false;
	});

	serial::interface.register_handler([](uint16_t data_id){
		serial::interface.update_data(slots::WEATHER_INFO, (uint8_t *)&weather::info, sizeof(weather::info));
		switch (data_id) {
			case slots::WEATHER_ICON:
				serial::interface.update_data(slots::WEATHER_ICON, (uint8_t *)weather::icon,  strlen(weather::icon));
				break;
			case slots::WEATHER_INFO:
				serial::interface.update_data(slots::WEATHER_INFO, (uint8_t *)&weather::info, sizeof(weather::info));
				break;
			default:
				break;
		}
	});
}

void weather::loop() {
	if (time_since_last_update == 0 || (now() - time_since_last_update) > 135) {
		// Do a weather update.
		
		if (!wifi::available()) return;
		if (config::manager.get_value(config::WEATHER_KEY) == nullptr) return;

		char url[128];
		snprintf(url, 128, "/forecast/%s/%s,%s?exclude=hourly,alerts&units=ca", 
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
				else if (stack_ptr == 3 && strcmp(stack[1]->name, "minutely") == 0) {
					if (strcmp(stack[2]->name, "summary") == 0 && v.type == json::Value::STR) {
						weather::echo_index = 0;
						weather::buffer_size = strlen(v.str_val) + 1;
						weather::info_buffer = (char*)realloc(weather::info_buffer, weather::buffer_size);
						strcpy(weather::info_buffer, v.str_val);
						Serial1.printf("summary = %s\n", v.str_val);
					}
				}
				else if (stack_ptr == 4 && strcmp(stack[1]->name, "daily") == 0 && stack[2]->is_array() && strcmp(stack[2]->name, "data") == 0 && stack[2]->index == 0) {
					if (strcmp(stack[3]->name, "apparentTemperatureHigh") == 0 && v.type == json::Value::FLOAT) {
						// Store the high apparent temperature
						weather::info.htemp = v.float_val;
						Serial1.printf("htemp = %f\n", v.float_val);
					}
					else if (strcmp(stack[3]->name, "apparentTemperatureLow") == 0 && v.type == json::Value::FLOAT) {
						// Store the low apparent temperature
						weather::info.ltemp = v.float_val;
						Serial1.printf("ltemp = %f\n", v.float_val);
					}
				}
		});
		
		json.parse(down.buf, down.length);

		time_since_last_update = now();

		serial::interface.update_data(slots::WEATHER_ICON, (uint8_t *)weather::icon,  strlen(weather::icon));
		serial::interface.update_data(slots::WEATHER_INFO, (uint8_t *)&weather::info, sizeof(weather::info));

		free(down.buf);
	}
}
