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

const static char* const TAG = "weather";

namespace weather {
	void init() {
	}

	int16_t cvt_temp(float t) {
		return (t * 100);
	}

	slots::WeatherStateCode from_api(int code) {
		using WS = slots::WeatherStateCode;

		switch (code) {
			case 1000: return WS::CLEAR;
			case 1001: return WS::CLOUDY;

			case 1100: return WS::CLEAR;
			case 1101: return WS::PARTLY_CLOUDY;
			case 1102: return WS::MOSTLY_CLOUDY;

			case 2000: return WS::FOG;
			case 2100: return WS::LIGHT_FOG;

			case 4000: return WS::DRIZZLE;
			case 4001: return WS::RAIN;
			case 4200: return WS::LIGHT_RAIN;
			case 4201: return WS::HEAVY_RAIN;

			case 5000: return WS::SNOW;
			case 5001: return WS::FLURRIES;
			case 5100: return WS::LIGHT_SNOW;
			case 5101: return WS::HEAVY_SNOW;

			case 6000: return WS::FREEZING_DRIZZLE;
			case 6001: return WS::FREEZING_RAIN;
			case 6200: return WS::FREEZING_LIGHT_RAIN;
			case 6201: return WS::FREEZING_HEAVY_RAIN;

			case 7000: return WS::ICE_PELLETS;
			case 7101: return WS::HEAVY_ICE_PELLETS;
			case 7102: return WS::LIGHT_ICE_PELLETS;

			case 8000: return WS::THUNDERSTORM;

			default: 
				ESP_LOGW(TAG, "Unexpected weather code %d", code);
				return WS::UNK;
		}
	}

	bool common_process_precip(std::unique_ptr<slots::PrecipData []>& target, int index, int max, const char *key, const json::Value& v) {
		if (strcmp(key, "precipitationProbability") == 0 && v.is_number() && v.as_number() > 0.f) {
			if (!target) target.reset(new slots::PrecipData[max]{});
			target[index].probability = uint8_t(v.as_number() * 2.55f);
			return true;
		}
		else {
			static const char * const names[] = {"rainIntensity", "snowIntensity", "sleetIntensity", "freezingRainIntensity"};
			static const slots::PrecipData::PrecipType codes[] = {slots::PrecipData::RAIN, slots::PrecipData::SNOW, slots::PrecipData::SLEET, slots::PrecipData::FREEZING_RAIN};
			for (int k = 0; k < 4; ++k) {
				if (strcmp(key, names[k]) == 0 && v.is_number() && v.as_number() > 0.f) {
					if (!target) target.reset(new slots::PrecipData[max]{});
					else if ((int16_t)(v.as_number() * 100) < target[index].amount) {}
					else {
						target[index].amount = v.as_number() * 100;
						target[index].kind = codes[k];
						return true;
					}
				}
			}
		}
		return false;
	}

	bool loop() {
		if (!api_key) return true;

		char url[128];
		snprintf(url, 128, "/v4/weather/forecast?location=%f,%f&apikey=%s", 
				lat,
				lon,
				api_key.get());

		auto dw = dwhttp::download_with_callback("_api.tomorrow.io", url); // leading _ indicates https

		if (dw.result_code() < 200 || dw.result_code() >= 300) {
			ESP_LOGW(TAG, "Got status code %d", dw.result_code());
			return false;
		}

		slots::WeatherInfo info{};
		slots::WeatherTimes suntimes;
		slots::WeatherStateCode state_data[24];
		int16_t temp_over_day[24]{};
		int16_t rtemp_over_day[24]{};
		int16_t wind_over_day[24]{};
		std::unique_ptr<slots::PrecipData []> hourly_precip;
		std::unique_ptr<slots::PrecipData []> minutely_precip;

		json::JSONParser w_parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v) {
			if (stack_ptr != 5) return;
			if (strcmp(stack[1]->name, "timelines")) return;
			if (!stack[2]->is_array()) return;
			if (strcmp(stack[3]->name, "values")) return;

			// Fill in the current weather data from the most current minute-by-minute data
			if (!strcmp(stack[2]->name, "minutely") && stack[2]->index == 0) {
				if (strcmp(stack[4]->name, "weatherCode") == 0 && v.type == json::Value::INT) {
					info.icon  = from_api(v.int_val);
				}
				else if (strcmp(stack[4]->name, "temperature") == 0 && v.is_number()) {
					info.crtemp = cvt_temp(v.as_number());
				}
				else if (strcmp(stack[4]->name, "temperatureApparent") == 0 && v.is_number()) {
					info.ctemp = cvt_temp(v.as_number());
				}
			}

			// Fill in minute-by-minute precipitation data.

			if (!strcmp(stack[2]->name, "minutely") && stack[2]->index < 60 && stack[2]->index % 2 == 0) {
				common_process_precip(minutely_precip, stack[2]->index / 2, 30, stack[4]->name, v);
			}

			// Fill in hourly data

			if (!strcmp(stack[2]->name, "hourly") && stack[2]->index < 24) {
				if (common_process_precip(hourly_precip, stack[2]->index, 24, stack[4]->name, v)) {}
				else if (strcmp(stack[4]->name, "weatherCode") == 0 && v.type == json::Value::INT) {
					state_data[stack[2]->index] = from_api(v.int_val);
				}
				else if (strcmp(stack[4]->name, "temperature") == 0 && v.is_number()) {
					rtemp_over_day[stack[2]->index] = cvt_temp(v.as_number());
				}
				else if (strcmp(stack[4]->name, "temperatureApparent") == 0 && v.is_number()) {
					temp_over_day[stack[2]->index] = cvt_temp(v.as_number());
				}
				else if (strcmp(stack[4]->name, "windSpeed") == 0 && v.is_number()) {
					wind_over_day[stack[2]->index] = v.as_number() * 360;
				}
			}

			// Fill in min/max/times from daily block 0
			if (strcmp(stack[2]->name, "daily") == 0 && stack[2]->index == 0) {
				if (strcmp(stack[4]->name, "temperatureApparentMin") == 0&& v.is_number()) {
					info.ltemp = cvt_temp(v.as_number());
				}
				else if (strcmp(stack[4]->name, "temperatureApparentMax") == 0&& v.is_number()) {
					info.htemp = cvt_temp(v.as_number());
				}
				else if (strcmp(stack[4]->name, "sunriseTime") == 0 && v.type == json::Value::STR) {
					suntimes.sunrise = wifi::from_iso8601(v.str_val) % (86400*1000);
				}
				else if (strcmp(stack[4]->name, "sunsetTime") == 0 && v.type == json::Value::STR) {
					suntimes.sunset = wifi::from_iso8601(v.str_val) % (86400*1000);
				}
			}
		});

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
		serial::interface.update_slot_nosync(slots::WEATHER_STATUS, "coming soon");

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
}
