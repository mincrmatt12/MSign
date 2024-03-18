#include "weather.h"
#include "../config.h"
#include "../serial.h"
#include "../dwhttp.h"
#include "../json.h"
#include "../wifitime.h"
#include <memory>
#include <optional>
#include <string.h>
#include <esp_log.h>
#include "weather.cfg.h"
#include "weathersummarizer.h"

const static char* const TAG = "weather";

namespace weather {
	void init() {
	}

	auto make_precip_appender(slots::DataID did, slots::WeatherInfo &out) {
		serial::interface.delete_slot(did);
		(did == slots::WEATHER_HPREC_GRAPH ? out.hour_precip_offset : out.minute_precip_offset) = 0xff;
		return [did, len = (uint8_t)0, &out](int offset, const slots::PrecipData& in) mutable {
			auto& allocated_offset = (did == slots::WEATHER_HPREC_GRAPH ? out.hour_precip_offset : out.minute_precip_offset);
			const uint8_t max = (did == slots::WEATHER_HPREC_GRAPH ? 120 : 72);
			if (in.probability > 0 && in.amount > 0) {
				if (allocated_offset == 0xff) {
					allocated_offset = offset;
				}

				int real_offset = offset - allocated_offset;
				while (real_offset >= len) {
					int oldlen = std::exchange(len, std::max(len + 20, real_offset + 5));
					if (len + allocated_offset > max) len = max - allocated_offset;
					serial::interface.allocate_slot_size(did, len * sizeof(slots::PrecipData));
					if (real_offset != oldlen) {
						const slots::PrecipData null{};
						for (int fill_offset = oldlen; fill_offset < real_offset; ++fill_offset)
							serial::interface.update_slot_at_inline(did, null, fill_offset);
					}
				}

				serial::interface.update_slot_at_inline(did, in, real_offset);
			}
			else if (allocated_offset != 0xff && len > (offset - allocated_offset)) {
				serial::interface.allocate_slot_size(did, (len = (offset - allocated_offset)) * sizeof(slots::PrecipData));
			}
		};
	}

	bool loop() {
		if (!api_key) return true;

		std::optional<dwhttp::Connection> dw;

		{
			char url[128];
			snprintf(url, 128, "/v4/timelines?apikey=%s", 
					api_key.get());

			dw = dwhttp::open_site("_api.tomorrow.io", url, "POST"); // leading _ indicates https
		}

		dw->send_header("Content-Type", "application/json");
		
		dw->start_body();
		{
			char buf[128];
			snprintf(buf, sizeof buf, R"({"location":[%f,%f])", lat, lon);
			dw->write(buf);
		}
		dw->write(R"(, "fields": [
	"freezingRainIntensity",
	"humidity",
	"precipitationProbability",
	"rainIntensity",
	"sleetIntensity",
	"snowDepth",
	"snowIntensity",
	"sunriseTime",
	"sunsetTime",
	"temperature",
	"temperatureApparent",
	"temperatureApparentMax",
	"temperatureApparentMin",
	"visibility",
	"weatherCode",
	"windGust",
	"windSpeed"
],
"units": "metric",
"timesteps": [
	"5m",
	"1h",
	"1d"
],
"endTime": "nowPlus5d"
})");
		dw->end_body();

		if (dw->result_code() < 200 || dw->result_code() >= 300) {
			ESP_LOGW(TAG, "Got status code %d", dw->result_code());
			return false;
		}

		PrecipitationSummarizer hourly_precip_summarizer{true}, minutely_precip_summarizer{false};
		HourlyConditionSummarizer hourly_overall_summarizer{};
		slots::WeatherInfo current_conditions{};
		{
			slots::WeatherDay current_day{};
			SingleDatapoint current_data{};
			int day_count = 0;

			current_conditions.updated_at = wifi::get_localtime();

			auto append_minute_precip = make_precip_appender(slots::WEATHER_MPREC_GRAPH, current_conditions);
			auto append_hour_precip = make_precip_appender(slots::WEATHER_HPREC_GRAPH, current_conditions);

			enum {
				DAILY,
				HOURLY,
				FIVEMINUTELY,
				OTHER
			} current_timeline = OTHER;

			json::JSONParser w_parser([&](json::PathNode ** stack, uint8_t stack_ptr, const json::Value& v) {
				if (stack_ptr < 4)
					return;

				// Only process values in the data block.
				if (strcmp(stack[1]->name, "data") || strcmp(stack[2]->name, "timelines")) {
					return;
				}

				// If this is the start of a data block, record which one it is.
				if (stack_ptr == 4 && !strcmp(stack[3]->name, "timestep") && v.type == v.STR) {
					current_timeline = OTHER;
					if (!strcmp(v.str_val, "1h")) current_timeline = HOURLY;
					else if (!strcmp(v.str_val, "5m")) current_timeline = FIVEMINUTELY;
					else if (!strcmp(v.str_val, "1d")) current_timeline = DAILY;

					return;
				}

				if (stack_ptr < 5 || strcmp(stack[4]->name, "values") || !stack[3]->is_array())
					return;

				if (stack_ptr == 5) {
					// If we've processed an entire datapoint, process it.
					current_data.recalculate_weather_code();
					switch (current_timeline) {
						case DAILY:
							{
								// Pull the precipitation data out of the general datapoint
								// (this resolves maximum according to a blogpost)
								current_day.precipitation = current_data.precipitation;

								if (stack[3]->index < 6) {
									serial::interface.update_slot_at(slots::WEATHER_DAYS, current_day, stack[3]->index, true, false);
									day_count = stack[3]->index + 1;
								}
							}
							break;
						case FIVEMINUTELY:
							{
								if (stack[3]->index < 120) {
									minutely_precip_summarizer.append(stack[3]->index, current_data);
								}
								if (stack[3]->index < 72) {
									append_minute_precip(stack[3]->index, current_data.precipitation);
								}
								if (stack[3]->index == 0) {
									current_conditions.wind_speed = current_data.wind_speed;
									current_conditions.wind_gust = current_data.wind_gust;
									current_conditions.visibility = current_data.visibility;
									current_conditions.relative_humidity = current_data.relative_humidity;
									current_conditions.ctemp = current_data.apparent_temperature;
									current_conditions.crtemp = current_data.temperature;
									current_conditions.icon = current_data.weather_code;
								}
							}
							break;
						case HOURLY:
							{
								if (stack[3]->index < 36) {
									hourly_precip_summarizer.append(stack[3]->index, current_data);
									hourly_overall_summarizer.append(stack[3]->index, current_data);
								}

								if (stack[3]->index < 120) {
									serial::interface.update_slot_at_inline(slots::WEATHER_ARRAY, current_data.weather_code, stack[3]->index);
									serial::interface.update_slot_at_inline(slots::WEATHER_TEMP_GRAPH, current_data.apparent_temperature, stack[3]->index);
									serial::interface.update_slot_at_inline(slots::WEATHER_RTEMP_GRAPH, current_data.temperature, stack[3]->index);
									serial::interface.update_slot_at_inline(slots::WEATHER_WIND_GRAPH, current_data.wind_speed, stack[3]->index);
									serial::interface.update_slot_at_inline(slots::WEATHER_GUST_GRAPH, current_data.wind_gust, stack[3]->index);
									append_hour_precip(stack[3]->index, current_data.precipitation);
								}
							}
						default:
							break;
					}
					
					// Reset the datapoint for the next entry
					current_data = {};
				}
				else if (stack_ptr == 6) {
					// Add data to the datapoint object.
					current_data.load_from_json(stack[5]->name, v);

					// If this is a day object, fill in the days array as well:
					if (current_timeline == DAILY && stack[3]->index <= 5) {
						if (strcmp(stack[5]->name, "temperatureApparentMin") == 0 && v.is_number()) {
							current_day.low_temperature = v.as_number() * 100;
						}
						else if (strcmp(stack[5]->name, "temperatureApparentMax") == 0 && v.is_number()) {
							current_day.high_temperature = v.as_number() * 100;
						}
						else if (strcmp(stack[5]->name, "sunriseTime") == 0 && v.type == json::Value::STR) {
							current_day.sunrise = wifi::from_iso8601(v.str_val) % (86400*1000);
						}
						else if (strcmp(stack[5]->name, "sunsetTime") == 0 && v.type == json::Value::STR) {
							current_day.sunset = wifi::from_iso8601(v.str_val) % (86400*1000);
						}
					}
				}
			});

			serial::interface.allocate_slot_size(slots::WEATHER_DAYS, sizeof(slots::WeatherDay) * 6);
			serial::interface.allocate_slot_size(slots::WEATHER_ARRAY, sizeof(slots::WeatherStateCode) * 24 * 5);
			serial::interface.allocate_slot_size(slots::WEATHER_TEMP_GRAPH, sizeof(int16_t) * 24 * 5);
			serial::interface.allocate_slot_size(slots::WEATHER_RTEMP_GRAPH, sizeof(int16_t) * 24 * 5);
			serial::interface.allocate_slot_size(slots::WEATHER_WIND_GRAPH, sizeof(int16_t) * 24 * 5);
			serial::interface.allocate_slot_size(slots::WEATHER_GUST_GRAPH, sizeof(int16_t) * 24 * 5);

			if (!w_parser.parse(*dw)) {
				ESP_LOGD(TAG, "Json parse failed");
				return true;
			}

			serial::interface.allocate_slot_size(slots::WEATHER_DAYS, sizeof(slots::WeatherDay) * day_count);
		}

		serial::interface.trigger_slot_update(slots::WEATHER_DAYS);
		serial::interface.trigger_slot_update(slots::WEATHER_ARRAY);
		serial::interface.trigger_slot_update(slots::WEATHER_TEMP_GRAPH);
		serial::interface.trigger_slot_update(slots::WEATHER_RTEMP_GRAPH);
		serial::interface.trigger_slot_update(slots::WEATHER_WIND_GRAPH);
		serial::interface.trigger_slot_update(slots::WEATHER_GUST_GRAPH);
		serial::interface.trigger_slot_update(slots::WEATHER_HPREC_GRAPH);
		serial::interface.trigger_slot_update(slots::WEATHER_MPREC_GRAPH);
		serial::interface.update_slot(slots::WEATHER_INFO, current_conditions);

		{
			const int size_of_buf = 384;
			auto working_buf = std::make_unique<char[]>(size_of_buf);
			SummaryResult code = generate_summary(minutely_precip_summarizer, hourly_precip_summarizer, working_buf.get(), size_of_buf);
			enum {
				USE_BUF,
				APPEND_HOUR_TO_BUF,
				PREPEND_HOUR_TO_BUF
			} mode = USE_BUF;
			switch (code) {
				case SummaryResult::Empty:
					// No summary can be created from precipitation, use the overall summary instead.
					hourly_overall_summarizer.generate_summary(current_conditions.icon, code, working_buf.get(), size_of_buf);
					break;
				case SummaryResult::PartialSummary:
					if (hourly_overall_summarizer.should_ignore_hourly_summary(current_conditions.icon)) {
						break;
					}
					mode = APPEND_HOUR_TO_BUF;
					[[fallthrough]];
				case SummaryResult::TotalSummary:
					if (hourly_overall_summarizer.has_important_message(current_conditions.icon)) {
						mode = PREPEND_HOUR_TO_BUF;
					}
					break;
			}

			switch (mode) {
				case APPEND_HOUR_TO_BUF:
					{
						serial::interface.update_slot(slots::WEATHER_STATUS, working_buf.get());
						int length = strlen(working_buf.get());
						hourly_overall_summarizer.generate_summary(current_conditions.icon, code, working_buf.get(), size_of_buf);
						serial::interface.allocate_slot_size(slots::WEATHER_STATUS, length + strlen(working_buf.get()) + 2);
						serial::interface.update_slot_partial(slots::WEATHER_STATUS, length, " ", 2, false, false); // Add a space.
						++length;
						serial::interface.update_slot_partial(slots::WEATHER_STATUS, length, working_buf.get(), strlen(working_buf.get()) + 1); // Add rest of summary
					}
					break;
				case PREPEND_HOUR_TO_BUF:
					{
						int length = strlen(working_buf.get()) + 1;
						memmove(working_buf.get() + size_of_buf - length, working_buf.get(), length);
						hourly_overall_summarizer.generate_summary(current_conditions.icon, code, working_buf.get(), size_of_buf - length);
						int hlength = strlen(working_buf.get());
						working_buf[hlength] = ' ';
						memmove(working_buf.get() + hlength + 1, working_buf.get() + size_of_buf - length, length);
					}
					[[fallthrough]];
				case USE_BUF:
					serial::interface.update_slot(slots::WEATHER_STATUS, working_buf.get());
					break;
			}
		}
		
		return true;
	}
}
