#include "weather.h"
#include "../config.h"
#include "../serial.h"
#include "../dwhttp.h"
#include "../json.h"
#include "../wifitime.h"
#include <string.h>
#include <esp_log.h>

const static char* TAG = "weather";

void weather::init() {
}

bool weather::loop() {
	// Do a weather update.
	
	if (config::manager.get_value(config::WEATHER_KEY) == nullptr) return true;

	char url[128];
	snprintf(url, 128, "/forecast/%s/%s,%s?exclude=alerts&units=ca", 
			config::manager.get_value(config::WEATHER_KEY),
			config::manager.get_value(config::WEATHER_LAT),
			config::manager.get_value(config::WEATHER_LONG));

	int16_t status_code;
	auto cb = dwhttp::download_with_callback("_api.darksky.net", url, status_code); // leading _ indicates https

	if (status_code < 200 || status_code >= 300) {
		dwhttp::stop_download();
		ESP_LOGW(TAG, "Got status code %d", status_code);
		return false;
	}

	slots::WeatherInfo info;
	slots::WeatherTimes suntimes;
	slots::WeatherStateArrayCode state_data[24];
	float temp_over_day[24];
	bool use_next_hour_summary = false;

	json::JSONParser w_parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v) {
		// First, check if we're in the currently block.
		
		if (stack_ptr == 3 && strcmp(stack[1]->name, "currently") == 0) {
			// Alright, we need to pull out:
			//
			// Current temp
			// Icon (special: if it's partly-cloudy, just mark that)
			
			if (strcmp(stack[2]->name, "apparentTemperature") == 0 && v.is_number()) {
				// Store the apparent temperature
				info.ctemp = v.as_number();
				ESP_LOGD(TAG, "temp = %f", v.float_val);
			}
			else if (strcmp(stack[2]->name, "icon") == 0 && v.type == json::Value::STR) {
				serial::interface.update_slot(slots::WEATHER_ICON, v.str_val);
				ESP_LOGD(TAG, "wicon = %s", v.str_val);
			}
			else if (strcmp(stack[2]->name, "temperature") == 0 && v.is_number()) {
				// Store the apparent temperature
				info.crtemp = v.as_number();
				ESP_LOGD(TAG, "rtemp = %f", v.float_val);
			}
		}
		else if (stack_ptr == 3 && strcmp(stack[1]->name, "minutely") == 0) {
			if (strcmp(stack[2]->name, "summary") == 0 && v.type == json::Value::STR) {
				serial::interface.update_slot(slots::WEATHER_STATUS, v.str_val);
				ESP_LOGD(TAG, "summary (minute) = %s", v.str_val);
			}
			else if (strcmp(stack[2]->name, "icon") == 0 && v.type == json::Value::STR) {
				if ( /* list of things that we should probably show an hourly summary for */
					!strcmp(v.str_val, "rain") || !strcmp(v.str_val, "snow") || !strcmp(v.str_val, "sleet")) {
					use_next_hour_summary = true;
					ESP_LOGD(TAG, "marking summary as hourly");
				}
			}
		}
		else if (stack_ptr == 3 && strcmp(stack[1]->name, "hourly") == 0) {
			if (strcmp(stack[2]->name, "summary") == 0 && v.type == json::Value::STR && !use_next_hour_summary) {
				serial::interface.update_slot(slots::WEATHER_STATUS, v.str_val);
				ESP_LOGD(TAG, "summary (hour) = %s", v.str_val);
			}
		}
		else if (stack_ptr == 4 && strcmp(stack[1]->name, "hourly") == 0 && strcmp(stack[2]->name, "data") == 0 && stack[2]->is_array() &&
				stack[2]->index < 24) {
			if (strcmp(stack[3]->name, "apparentTemperature") == 0 && v.is_number()) {
				temp_over_day[stack[2]->index] = v.as_number();
			}
			else if (strcmp(stack[3]->name, "icon") == 0 && v.type == json::Value::STR) {
				// Part 1 of algorithm; assume we get icon first and set upper nybble
				
				if (strncmp(v.str_val, "partly-cloudy", 13) == 0) {
					state_data[stack[2]->index] = slots::WeatherStateArrayCode::PARTLY_CLOUDY;
				}
				else if (strncmp(v.str_val, "clear", 5) == 0) {
					state_data[stack[2]->index] = slots::WeatherStateArrayCode::CLEAR;
				}
				else if (strcmp(v.str_val, "snow") == 0) {
					state_data[stack[2]->index] = slots::WeatherStateArrayCode::SNOW;
				}
				else if (strcmp(v.str_val, "cloudy") == 0) {
					state_data[stack[2]->index] = slots::WeatherStateArrayCode::MOSTLY_CLOUDY;
				}
				else if (strcmp(v.str_val, "rain") == 0) {
					state_data[stack[2]->index] = slots::WeatherStateArrayCode::DRIZZLE;
				}
			}
			else if (strcmp(stack[3]->name, "cloudCover") == 0 && v.is_number()) {
				// Part 2a: is the thing a cloud?
				if (((uint8_t)state_data[stack[2]->index] & 0xf0) == (uint8_t)slots::WeatherStateArrayCode::CLEAR && state_data[stack[2]->index] != slots::WeatherStateArrayCode::CLEAR) {
					if (v.as_number() < 0.5) {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::PARTLY_CLOUDY;
					}
					else if (v.as_number() < 0.9) {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::MOSTLY_CLOUDY;
					}
					else {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::OVERCAST;
					}
				}
			}
			else if (strcmp(stack[3]->name, "precipIntensity") ==0 && v.is_number()) {
				// Part 2b: check precip type from icon
				if (((uint8_t)state_data[stack[2]->index] & 0xf0) == (uint8_t)slots::WeatherStateArrayCode::DRIZZLE) {
					if (v.as_number() < 0.4) {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::DRIZZLE;
					}
					else if (v.as_number() < 2.5) {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::LIGHT_RAIN;
					}
					else if (v.as_number() < 10) {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::RAIN;
					}
					else {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::HEAVY_RAIN;
					}
				}
				else if (((uint8_t)state_data[stack[2]->index] & 0xf0) == (uint8_t)slots::WeatherStateArrayCode::SNOW) {
					if (v.as_number() < 3) {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::SNOW;
					}
					else {
						state_data[stack[2]->index] = slots::WeatherStateArrayCode::HEAVY_SNOW;
					}
				}
			}
		}
		else if (stack_ptr == 4 && strcmp(stack[1]->name, "daily") == 0 && stack[2]->is_array() && strcmp(stack[2]->name, "data") == 0 && stack[2]->index == 0) {
			if (strcmp(stack[3]->name, "apparentTemperatureHigh") == 0 && v.is_number()) {
				// Store the high apparent temperature
				info.htemp = v.as_number();
				ESP_LOGD(TAG, "htemp = %f", v.as_number());
			}
			else if (strcmp(stack[3]->name, "apparentTemperatureLow") == 0 && v.is_number()) {
				// Store the low apparent temperature
				info.ltemp = v.as_number();
				ESP_LOGD(TAG, "ltemp = %f", v.as_number());
			}
			else if (strcmp(stack[3]->name, "sunriseTime") == 0 && v.type == json::Value::INT) {
				// Get the sunrise time
				suntimes.sunrise = (wifi::millis_to_local(v.int_val * 1000) % (86400*1000));
			}
			else if (strcmp(stack[3]->name, "sunsetTime") == 0 && v.type == json::Value::INT) {
				// Get the sunset time
				suntimes.sunset = (wifi::millis_to_local(v.int_val * 1000) % (86400*1000));
			}
		}
	}, true); // use utf8 fixup

	use_next_hour_summary = false;
	if (!w_parser.parse(std::move(cb))) {
		ESP_LOGD(TAG, "Json parse failed");
		dwhttp::stop_download();
		return true;
	}

	dwhttp::stop_download();
	
	// Update the resulting things
	serial::interface.update_slot_nosync(slots::WEATHER_INFO, info);
	serial::interface.update_slot(slots::WEATHER_ARRAY, state_data, sizeof state_data, false);
	serial::interface.update_slot(slots::WEATHER_TEMP_GRAPH, temp_over_day, sizeof temp_over_day, false);
	serial::interface.update_slot_nosync(slots::WEATHER_TIME_SUN, suntimes);

	// Sync
	serial::interface.sync_slots(slots::WEATHER_INFO, slots::WEATHER_ARRAY, slots::WEATHER_TEMP_GRAPH, slots::WEATHER_TIME_SUN);
	
	return true;
}
