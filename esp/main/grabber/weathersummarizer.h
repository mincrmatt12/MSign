#pragma once

#include "../common/slots.h"
#include "../json.h"

namespace weather {
	// Single data point from the JSON stream for minutely / hourly data.
	//
	// Units equivalent to slots::WeatherInfo
	struct SingleDatapoint {
		int16_t temperature, apparent_temperature;
		slots::PrecipData precipitation{};
		int16_t snow_depth{};

		int16_t wind_speed, wind_gust;

		slots::WeatherStateCode weather_code;
		uint8_t relative_humidity;

		int16_t visibility = -1;

		// Incorporates data from a JSON callback
		void load_from_json(const char *key, const json::Value &val);

		// Updates weather code to include things like wind conditions & humidity.
		//
		// Additionally, overrides probability on precipitation based on the weather status code if describing current
		// conditions or those in <10 minutes.
		void recalculate_weather_code(bool within_10_minutes);
	};

	// Data points for full days are stored in WeatherDay objects directly.
	
	// Usage hints for generated summary
	enum struct SummaryResult {
		Empty,             // No summary is present.
		TotalSummary,      // A summary was generated, but is complex enough to warrant using it on its own.
		PartialSummary,    // A summary was generated, but is short enough that a second sentence can be added to it.
	};
	
	// Summarizer for a stream of precipitation data.
	class PrecipitationSummarizer {
		struct Block {
			// Marks the start and end of the block indices, counting from any precipitation
			// detected at all. These are used to group large blocks of rain with bursts in the middle
			// of potential rain into a single block, while still allowing the message to include a range
			// of start times when ambiguous.
			//
			// Special values:
			// 	 -2: not opened
			// 	 -1: unbounded (only used for end; start == 0 is treated as unbounded on the left)
			int16_t left = -2, right = -2;

			// Marks the actual start and end indices of the block, counting from precipitation with probability
			// above 50.
			int16_t start = -2, end = -2;

			// Counts indices within the start--end range that have probability above 87; if this makes up a sizeable
			// part of that region, then the block is deemed likely, otherwise it is only describing possible precipitation.
			//
			// Probability above 95 counts as 3 entries here.
			uint16_t likely_count;

			// Add some precipitation data to the block. Returns true if the block is active following the append.
			bool append(uint16_t index, const slots::PrecipData& precipitation);

			bool is_likely(uint16_t fallback_threshold=3) const {
				if (start == -2)
					return false;

				if (end == -1)
					return likely_count > fallback_threshold;
				else
					return (likely_count >= (end - start) / 4);
			}

			static bool would_start_block(const slots::PrecipData& precipitation);
		} blocks[2]{}; // There are at most 2 blocks. In general, we try to maintain enough information to describe
					   // a period of intermittent rain either following or followed by another period of more consistent rain.
					   // To accomplish that goal, we fill up the array with blocks, and when it's full we condense the two closest
					   // adjacent blocks into one larger block with the intermittent flag set.

		enum IntermittentFlag : uint8_t {
			CONTINUOUS,

			INTERMITTENT_THEN_CONTINUOUS, // The first block describes a region of intermittent precipitation, followed by a continuous region of precipitation.
			CONTINUOUS_THEN_INTERMITTENT, // The first block describes a region of continuous precipitation, followed by another block of intermittent precipitation.

			CHUNKS_THEN_CONTINUOUS, // Same as INTERMITTENT_THEN_CONTINUOUS, but the blocks were grouped further apart.
			CONTINUOUS_THEN_CHUNKS, // Same as CONTINUOUS_THEN_INTERMITTENT, but the blocks were grouped further apart.

			INTERMITTENT            // Similar to INTERMITTENT_THEN_CONTINUOUS, but more than one intermittent region was found so the separation between the
									// intermittent region and the continuous one is ambiguous.
		} intermittent_flag = CONTINUOUS;

		bool block_is_active = false;
		uint8_t block_index = 0;

		// Ensure block_index points to a free block to append into. If the gap between the last block is small, a new block
		// is not started and block_index continues to point to an old block.
		void start_next_block(uint16_t index);

		int last_filled_block_index() const {
			if (block_is_active)
				return block_index;
			else
				return block_index - 1;
		}

		bool empty() const {
			return block_index == 0 && !block_is_active;
		}

		struct Amount {
			// Counts the two most interesting kinds of precipitation. The ordering is according to the value
			// of PrecipType, so RAIN > SNOW > FREEZING_RAIN > SLEET
			slots::PrecipData::PrecipType kinds[2]{};
			// Amount of precipitation for the corresponding type in kinds. mm/hr*100 for everything but snow which is
			// total snow on ground in mm.
			int16_t amount_mins[2], amount_maxs[2];

			// Number of times we've seen each of the precipitation types in kinds[]
			int16_t detected_counts[2];

			// Counts indices observed of precipitation not in the kinds[] array.
			int16_t mixed_count{}; 

			// Set to true if thunderstorms are detected. Modifies the textual descriptions of RAIN -> "thunderstorms" and SNOW -> "thundersnow" (unless
			// rain is also present, in which case it stays as snow).
			bool thunderstorms_present{};

			// Counts total indices observed with precipitation.
			int16_t total_count{};

			// Add some weather data to the amount tracker.
			void append(const SingleDatapoint& datapoint);

			int most_prevalent_index() const {
				if (kinds[0] != slots::PrecipData::NONE && kinds[1] != slots::PrecipData::NONE) {
					return (detected_counts[0] > detected_counts[1]) ? 0 : 1;
				}
				else return 0;
			}
		} amount{};

	public:
		// Adjusts some thresholding regarding indices for 1 idx = 1hr vs 1 idx = 5minute
		bool is_hourly = false;

		PrecipitationSummarizer() = default;
		PrecipitationSummarizer(bool is_hourly) : is_hourly(is_hourly) {}

		// Add a datapoint to the summary. Must be called with increasing indices with no gaps.
		void append(uint16_t index, const SingleDatapoint& datapoint);

		// Produce a summary contianing precipitation information. The summary is written to the provided buffer, which should ideally be relatively large as summaries
		// that don't fit are truncated.
		friend SummaryResult generate_summary(const PrecipitationSummarizer& minutely_summary, const PrecipitationSummarizer& hourly_summary, char * buf, size_t buflen);
		friend struct TimeSpec;
	};

	SummaryResult generate_summary(const PrecipitationSummarizer& minutely_summary, const PrecipitationSummarizer& hourly_summary, char * buf, size_t buflen);

	// Summarizes things that aren't precipitation-related over 36 hours.
	class HourlyConditionSummarizer {
		struct Wind {
			// Earliest time strong wind was observed. Negative if not observed yet.
			int16_t earliest = -1, peak, latest;

			// Strongest wind observed (if high enough we use this to augment the summary)
			int16_t wind_speed, wind_gust_speed;
		} wind{}; // wind is most intersting

		struct Clear {
			int16_t earliest = -1, count = 0;
		} clear{};

		struct Fog {
			int16_t earliest = -1, latest;

			slots::WeatherStateCode strongest_fog = slots::WeatherStateCode::LIGHT_FOG;
		} fog{};

		template<typename T>
		static bool is_present(const T& cond_struct) {
			return cond_struct.earliest != -1;
		}

		template<typename T>
		static bool starting_later(const T& cond_struct) {
			return cond_struct.earliest > 0;
		}
	public:
		// Add a datapoint to the summary. Must be called with increasing indices with no gaps.
		void append(uint16_t index, const SingleDatapoint& datapoint);

		// Types of hourly summaries
		enum HourlySummaryType {
			WINDY_SOON, // "Very windy in 2 hours"
			WINDY_STOPPING, // Very windy conditions stopping in 5 hours"

			EXTREMELY_WINDY_SOON, // Dangerously windy in 5 hours
			EXTREMELY_WINDY_STOPPING, // Dangerously windy conditions stopping this evening"

			FOG_CLEARING_UP, // Light fog clearing up in ~3 hours
			FOG_LATER, // Foggy this evening
			
			CLOUDS_CLEARING, // Partly cloudy, clearing up in ~2 hours

			CURRENT_CONDITIONS // Currently cloudy.
		};

		HourlySummaryType current_summary_type(slots::WeatherStateCode current_code) const;

		bool has_important_message(slots::WeatherStateCode current_code) const {
			switch (current_summary_type(current_code)) {
				case EXTREMELY_WINDY_STOPPING:
				case WINDY_STOPPING:
					return true;
				default:
					return false;
			}
		}

		bool should_ignore_hourly_summary(slots::WeatherStateCode current_code) const {
			if (current_summary_type(current_code) == CURRENT_CONDITIONS)
				return false;
			else switch (current_code) {
				case slots::WeatherStateCode::DRIZZLE:
				case slots::WeatherStateCode::LIGHT_RAIN:
				case slots::WeatherStateCode::RAIN:
				case slots::WeatherStateCode::HEAVY_RAIN:
				case slots::WeatherStateCode::SNOW:
				case slots::WeatherStateCode::FLURRIES:
				case slots::WeatherStateCode::LIGHT_SNOW:
				case slots::WeatherStateCode::HEAVY_SNOW:
				case slots::WeatherStateCode::FREEZING_DRIZZLE:
				case slots::WeatherStateCode::FREEZING_LIGHT_RAIN:
				case slots::WeatherStateCode::FREEZING_RAIN:
				case slots::WeatherStateCode::FREEZING_HEAVY_RAIN:
				case slots::WeatherStateCode::LIGHT_ICE_PELLETS:
				case slots::WeatherStateCode::ICE_PELLETS:
				case slots::WeatherStateCode::HEAVY_ICE_PELLETS:
				case slots::WeatherStateCode::THUNDERSTORM:
					return true;
				default:
					return false;
			}
		}

		// Produce a summary containing general hourly information.
		void generate_summary(slots::WeatherStateCode current_code, SummaryResult prior_result, char *buf, size_t buflen) const;
	};
};
