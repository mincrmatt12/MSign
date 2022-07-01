#include "weather.h"
#include "../config.h"
#include "../serial.h"
#include "../dwhttp.h"
#include "../json.h"
#include "../wifitime.h"
#include <memory>
#include <string.h>
#include <esp_log.h>
#include "weather.cfg.h"

const static char* TAG = "weather";

void weather::init() {
}

namespace weather {
	int16_t cvt_temp(float t) {
		return (t * 100);
	}
}

bool weather::loop() {
	// Do a weather update.
	
	if (!api_key) return true;

	char url[128];
	snprintf(url, 128, "/forecast/%s/%f,%f?exclude=alerts&units=ca", 
			api_key.get(),
			lat,
			lon);

	auto dw = dwhttp::download_with_callback("_api.darksky.net", url); // leading _ indicates https

	if (dw.result_code() < 200 || dw.result_code() >= 300) {
		ESP_LOGW(TAG, "Got status code %d", dw.result_code());
		return false;
	}

	slots::WeatherInfo info{};
	slots::WeatherTimes suntimes;
	slots::WeatherStateArrayCode state_data[24];
	int16_t temp_over_day[24]{};
	int16_t rtemp_over_day[24]{};
	int16_t wind_over_day[24]{};
	std::unique_ptr<slots::PrecipData []> hourly_precip;
	std::unique_ptr<slots::PrecipData []> minutely_precip;
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
				info.ctemp = cvt_temp(v.as_number());
				ESP_LOGD(TAG, "temp = %f", v.float_val);
			}
			else if (strcmp(stack[2]->name, "icon") == 0 && v.type == json::Value::STR) {
				serial::interface.update_slot(slots::WEATHER_ICON, v.str_val);
				ESP_LOGD(TAG, "wicon = %s", v.str_val);
			}
			else if (strcmp(stack[2]->name, "temperature") == 0 && v.is_number()) {
				// Store the apparent temperature
				info.crtemp = cvt_temp(v.as_number());
				ESP_LOGD(TAG, "rtemp = %f", v.float_val);
			}
		}
		else if (stack_ptr == 3 && strcmp(stack[1]->name, "minutely") == 0) {
			if (strcmp(stack[2]->name, "summary") == 0 && v.type == json::Value::STR) {
				serial::interface.update_slot_nodirty(slots::WEATHER_STATUS, v.str_val);
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
				serial::interface.update_slot_nodirty(slots::WEATHER_STATUS, v.str_val);
				ESP_LOGD(TAG, "summary (hour) = %s", v.str_val);
			}
		}
		else if (stack_ptr == 4 && strcmp(stack[1]->name, "hourly") == 0 && strcmp(stack[2]->name, "data") == 0 && stack[2]->is_array() &&
				stack[2]->index < 24) {
			if (strcmp(stack[3]->name, "apparentTemperature") == 0 && v.is_number()) {
				temp_over_day[stack[2]->index] = cvt_temp(v.as_number());
			}
			else if (strcmp(stack[3]->name, "temperature") == 0 && v.is_number()) {
				rtemp_over_day[stack[2]->index] = cvt_temp(v.as_number());
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
				else if (strcmp(v.str_val, "fog") == 0) {
					state_data[stack[2]->index] = slots::WeatherStateArrayCode::FOG;
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
					else if (v.as_number() < 5) {
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

				if (v.as_number() > 0.f) {
					// Additionally, load into/create precip array
					if (!hourly_precip) hourly_precip.reset(new slots::PrecipData[24]{});
					hourly_precip[stack[2]->index].amount = cvt_temp(v.as_number());
				}
			}
			else if (strcmp(stack[3]->name, "precipIntensityError") == 0 && v.is_number()) {
				if (!hourly_precip) hourly_precip.reset(new slots::PrecipData[24]{});
				hourly_precip[stack[2]->index].stddev = cvt_temp(v.as_number());
			}
			else if (strcmp(stack[3]->name, "precipProbability") == 0 && v.is_number() && v.as_number() > 0.f) {
				if (!hourly_precip) hourly_precip.reset(new slots::PrecipData[24]{});
				hourly_precip[stack[2]->index].probability = uint8_t(v.as_number() * 255.f);
			}
			else if (strcmp(stack[3]->name, "precipType") == 0 && v.type == v.STR && strcmp(v.str_val, "rain")) {
				if (!hourly_precip) hourly_precip.reset(new slots::PrecipData[24]{});
				hourly_precip[stack[2]->index].is_snow = true;
			}
			else if (strcmp(stack[3]->name, "windSpeed") == 0 && v.is_number()) {
				wind_over_day[stack[2]->index] = v.as_number() * 100;
			}
		}
		else if (stack_ptr == 4 && strcmp(stack[1]->name, "minutely") == 0 && strcmp(stack[2]->name, "data") == 0 && stack[2]->is_array() && stack[2]->index < 60 && stack[2]->index % 2 == 0) {
			if (strcmp(stack[3]->name, "precipIntensity") ==0 && v.is_number() && v.as_number() > 0.f) {
				if (!minutely_precip) minutely_precip.reset(new slots::PrecipData[30]{});
				minutely_precip[stack[2]->index / 2].amount = cvt_temp(v.as_number());
				ESP_LOGD(TAG, "got minute precip %d = %d", stack[2]->index,	minutely_precip[stack[2]->index / 2].amount);
			}
			else if (strcmp(stack[3]->name, "precipIntensityError") == 0 && v.is_number()) {
				if (!minutely_precip) minutely_precip.reset(new slots::PrecipData[30]{});
				minutely_precip[stack[2]->index / 2].stddev = cvt_temp(v.as_number());
			}
			else if (strcmp(stack[3]->name, "precipProbability") == 0 && v.is_number() && v.as_number() > 0.f) {
				if (!minutely_precip) minutely_precip.reset(new slots::PrecipData[30]{});
				minutely_precip[stack[2]->index / 2].probability = uint8_t(v.as_number() * 255.f);
			}
			else if (strcmp(stack[3]->name, "precipType") == 0 && v.type == v.STR && strcmp(v.str_val, "rain")) {
				if (!minutely_precip) minutely_precip.reset(new slots::PrecipData[30]{});
				minutely_precip[stack[2]->index / 2].is_snow = true;
			}
		}
		else if (stack_ptr == 4 && strcmp(stack[1]->name, "daily") == 0 && stack[2]->is_array() && strcmp(stack[2]->name, "data") == 0 && stack[2]->index == 0) {
			if (strcmp(stack[3]->name, "apparentTemperatureHigh") == 0 && v.is_number()) {
				// Store the high apparent temperature
				info.htemp = cvt_temp(v.as_number());
				ESP_LOGD(TAG, "htemp = %f", v.as_number());
			}
			else if (strcmp(stack[3]->name, "apparentTemperatureLow") == 0 && v.is_number()) {
				// Store the low apparent temperature
				info.ltemp = cvt_temp(v.as_number());
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
	if (!w_parser.parse(dw)) {
		ESP_LOGD(TAG, "Json parse failed");
		return true;
	}
	
	// Update the resulting things
	serial::interface.update_slot_nosync(slots::WEATHER_INFO, info);
	serial::interface.update_slot_nosync(slots::WEATHER_ARRAY, state_data);
	serial::interface.update_slot_nosync(slots::WEATHER_TEMP_GRAPH, temp_over_day);
	serial::interface.update_slot_nosync(slots::WEATHER_TIME_SUN, suntimes);

	serial::interface.update_slot_nosync(slots::WEATHER_RTEMP_GRAPH, rtemp_over_day);
	serial::interface.update_slot_nosync(slots::WEATHER_WIND_GRAPH, wind_over_day);
	serial::interface.trigger_slot_update(slots::WEATHER_STATUS);

	if (minutely_precip) {
		if (std::none_of(minutely_precip.get(), minutely_precip.get() + 30, [](const auto& x){return x.amount > 0 && x.probability != 0;})) minutely_precip.reset();
	}
	if (hourly_precip) {
		if (std::none_of(hourly_precip.get(), hourly_precip.get() + 24, [](const auto& x){return x.amount > 0 && x.probability != 0;})) hourly_precip.reset();
	}

	if (minutely_precip) {
		serial::interface.update_slot_raw(slots::WEATHER_MPREC_GRAPH, minutely_precip.get(), sizeof(slots::PrecipData)*30, false);
	}
	else serial::interface.delete_slot(slots::WEATHER_MPREC_GRAPH);

	if (hourly_precip) {
		serial::interface.update_slot_raw(slots::WEATHER_HPREC_GRAPH, hourly_precip.get(), sizeof(slots::PrecipData)*24, false);
	}
	else serial::interface.delete_slot(slots::WEATHER_HPREC_GRAPH);

	// Sync
	serial::interface.sync();
	
	return true;
}
