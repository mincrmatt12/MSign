#include "weathersummarizer.h"

#ifdef WEATHERSUMMARIZER_TEST
#include <stdio.h>
#define ESP_LOGW(x, y, ...) fprintf(stderr, y, ##__VA_ARGS__)
#define ESP_LOGE(x, y, ...) fprintf(stderr, y, ##__VA_ARGS__)
#define ESP_LOGI(x, y, ...) fprintf(stderr, y, ##__VA_ARGS__)
#else
#include <esp_log.h>
#endif

#include <utility>
#include <time.h>
#include <ctype.h>

const static char* const TAG = "weathersummarizer";

namespace weather {
	slots::WeatherStateCode wsc_from_api(int code) {
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

	// Summarizer datapoints require:
	// [
	//   "rainIntensity", "snowIntensity", "sleetIntensity", "freezingRainIntensity",
	//   "precipitationProbability",
	//   "humidity",
	//   "weatherCode",
	//   "snowDepth",
	//   "windSpeed",
	//   "windGust",
	//   "temperature",
	//   "temperatureApparent",
	//   "visibility"
	// ]

	void SingleDatapoint::load_from_json(const char *key, const json::Value &v) {
		{
			static const char * const names[] = {"rainIntensity", "snowIntensity", "sleetIntensity", "freezingRainIntensity"};
			static const slots::PrecipData::PrecipType codes[] = {slots::PrecipData::RAIN, slots::PrecipData::SNOW, slots::PrecipData::SLEET, slots::PrecipData::FREEZING_RAIN};
			for (int k = 0; k < 4; ++k) {
				if (strcmp(key, names[k]) == 0) {
					if (v.is_number() && v.as_number() > 0.f) {
						auto raw = int16_t(v.as_number());
						if (raw > precipitation.amount || precipitation.kind == slots::PrecipData::NONE) {
							precipitation.amount = v.as_number() * 100;
							precipitation.kind = codes[k];
						}
					}
					return;
				}
			}
		}

		if (strcmp(key, "precipitationProbability") == 0 && v.is_number() && v.as_number() > 0.f) {
			precipitation.probability = uint8_t(v.as_number() * 2.55f);
		}
		if (strcmp(key, "humidity") == 0 && v.is_number()) {
			relative_humidity = uint8_t(v.as_number() * 2.55f);
		}
		else if (strcmp(key, "weatherCode") == 0 && v.type == json::Value::INT) {
			weather_code = wsc_from_api(v.int_val);
		}
		else if (strcmp(key, "snowDepth") == 0 && v.is_number()) {
			snow_depth = v.as_number() * 100;
		}
		else if (strcmp(key, "windSpeed") == 0 && v.is_number()) {
			wind_speed = v.as_number() * 360; // m/s --> 100*km/h 
		}
		else if (strcmp(key, "windGust") == 0 && v.is_number()) {
			wind_gust = v.as_number() * 360;
		}
		else if (strcmp(key, "temperature") == 0 && v.is_number()) {
			temperature = v.as_number() * 100;
		}
		else if (strcmp(key, "temperatureApparent") == 0 && v.is_number()) {
			apparent_temperature = v.as_number() * 100;
		}
		else if (strcmp(key, "visibility") == 0 && v.is_number()) {
			visibility = v.as_number() * 10;
		}
	}

	void SingleDatapoint::recalculate_weather_code() {
		// TODO: add humidity detection

		slots::WeatherStateCode category = static_cast<slots::WeatherStateCode>(uint8_t(weather_code) & 0xf0);

		// Only augment conditions that are less interesting than high wind / humidity.
		if (category != slots::WeatherStateCode::CLEAR && category != slots::WeatherStateCode::LIGHT_FOG)
			return;

		if (wind_speed > 4000 || wind_gust > 5000) // 55 km/h
			weather_code = slots::WeatherStateCode::WINDY;
	}

	bool PrecipitationSummarizer::Block::append(uint16_t index, const slots::PrecipData& precipitation) {
		if (!would_start_block(precipitation)) { // PRE: if block is unopened, would_start_block == true; thus, index - 1 > 0
			// This does not meet the threshold for starting a block, end the current block here if it is currently
			// open.
			if (right == -1)
				right = index;
			if (end == -1)
				end = index;

			return false; // Stop processing the datablock. Caller notices the false return and tries to append to a new block (unless
						  // the distance in indices from right is less than some threshold, in which case it can reopen the block).
		}

		// Precipitation exists in this block, if it is currently unopened, open it.
		if (left == -2) {
			left = index;
		}

		// The block has not ended yet, so mark it as unbounded on the right. If the caller decided to consolidate two very close blocks,
		// this will ensure they remain marked properly.
		right = -1;

		if (precipitation.probability > /* 50% */ 127) {
			// This is the start of "real precipitation".

			if (start == -2)
				start = index;

			end = -1;

			// Count likely precipitation.
			if (precipitation.probability > /* 87% */ 220)
				++likely_count;
			// Put greater emphasis on really likely events
			if (precipitation.probability > /* 94% */ 240)
				likely_count += 2;
		}
		else {
			// The block still extends past here, however it is now closed on the right for >50 precip
			if (end == -1)
				end = index - 1;
		}

		return true;
	}

	bool PrecipitationSummarizer::Block::would_start_block(const slots::PrecipData &precipitation) {
		return precipitation.kind != slots::PrecipData::PrecipType::NONE && precipitation.probability > 12;
	}

	void PrecipitationSummarizer::start_next_block(uint16_t index) {
		// If a block is currently active, just append into it.
		if (block_is_active)
			return;

		// Otherwise, if there is a previous block, check if it ended relatively soon before now.
		if (block_index > 0 && !is_hourly) {
			if (blocks[block_index - 1].right + 2 >= index) {
				// Re-use that block.
				--block_index;
				block_is_active = true;
				return;
			}
			else if (blocks[block_index - 1].right - blocks[block_index - 1].left < 3) {
				// If the previous block is too short, destroy it and replace it with this one.
				--block_index;
				blocks[block_index] = {};
				block_is_active = true;
				return;
			}
		}


		// Otherwise, if there is space for another block, add into it.
		if (block_index < 2) {
			block_is_active = true;
			return;
		}

		// Otherwise, block index is at 2. We have to determine how to describe the intermittent precipitation.
		switch (intermittent_flag) {
			case CONTINUOUS:
				// There are just two sections of continuous rain; decide which one is closer.
				if (index - blocks[1].right < blocks[1].left - blocks[0].right) {
					intermittent_flag = (index - blocks[1].right < (is_hourly ? 2 : 4)) ? CONTINUOUS_THEN_INTERMITTENT : CONTINUOUS_THEN_CHUNKS;
					break;
				}
				else {
					intermittent_flag = (blocks[1].left - blocks[0].right < (is_hourly ? 2 : 4)) ? 
						INTERMITTENT_THEN_CONTINUOUS : CHUNKS_THEN_CONTINUOUS;
					goto intermittent_after;
				}
			case CONTINUOUS_THEN_INTERMITTENT:
			case CONTINUOUS_THEN_CHUNKS:
				break;

			case INTERMITTENT_THEN_CONTINUOUS:
			case       CHUNKS_THEN_CONTINUOUS:
			case INTERMITTENT:
				intermittent_flag = INTERMITTENT;
			intermittent_after:
				blocks[0].end = blocks[1].end;
				blocks[0].right = blocks[1].right;
				blocks[1] = {};
				break;
		}

		block_index = 1;
		block_is_active = true;
	}

	void PrecipitationSummarizer::Amount::append(const SingleDatapoint& datapoint) {
		if (!Block::would_start_block(datapoint.precipitation))
			return;

		++total_count;

		int idx_to_use = -1;

		if (auto pos = std::find(std::begin(kinds), std::end(kinds), datapoint.precipitation.kind); pos != std::end(kinds)) {
			// If the precipitation is already present in the array, use it
			idx_to_use = pos - kinds;
		}
		else if (auto pos = std::find(std::begin(kinds), std::end(kinds), slots::PrecipData::NONE); pos != std::end(kinds)) {
			// If we haven't seen more than 2 kinds of precipitation, just append into the array directly.
			idx_to_use = pos - kinds;
		}
		else for (int i = 0; i < std::size(kinds); ++i) {
			// If we haven't found any less important conditions yet and the current condition
			// is less important, use it.
			if (idx_to_use == -1 && datapoint.precipitation.kind < kinds[i]) {
				idx_to_use = i;
			}
			// If we have found one replaceable entry and the current entry is less important, replace
			// it instead.
			else if (idx_to_use != -1 && kinds[i] > kinds[idx_to_use]) {
				idx_to_use = i;
			}
		}

		if (idx_to_use == -1) {
			// Precipitation was found, but we're not keeping track of it's value; instead just
			// append to mixed_count
			++mixed_count;
			return;
		}

		int16_t raw_amount = datapoint.precipitation.amount;
		if (datapoint.precipitation.kind == slots::PrecipData::SNOW) {
			raw_amount = datapoint.snow_depth;
		}

		if (kinds[idx_to_use] != datapoint.precipitation.kind) {
			// We're initializing a new weather condition type, reset min/max.
			amount_mins[idx_to_use] = amount_maxs[idx_to_use] = raw_amount;
			if (kinds[idx_to_use] != slots::PrecipData::NONE) {
				// We're overwriting a previously used type of precpitation, account for it in
				// the mixed_count
				mixed_count += std::exchange(detected_counts[idx_to_use], 1);
			}
			else {
				// Just reset detected counts to 1 if there wasn't anything in that slot previously
				detected_counts[idx_to_use] = 1;
			}
			kinds[idx_to_use] = datapoint.precipitation.kind;
		}
		else {
			// If there is already data in the slot, just update min/max & increase the count.
			++detected_counts[idx_to_use];
			amount_mins[idx_to_use] = std::min(amount_mins[idx_to_use], raw_amount);
			amount_maxs[idx_to_use] = std::max(amount_maxs[idx_to_use], raw_amount);
		}

		// If there are thunderstorms present in this entry, set the thunderstorm flag.
		if (datapoint.weather_code == slots::WeatherStateCode::THUNDERSTORM)
			thunderstorms_present = true;

		// Finally, ensure the kinds array is sorted.
		if (kinds[0] != slots::PrecipData::NONE && kinds[1] != slots::PrecipData::NONE && kinds[0] > kinds[1]) {
			std::swap(kinds[0], kinds[1]);
			std::swap(detected_counts[0], detected_counts[1]);
			std::swap(amount_mins[0], amount_mins[1]);
			std::swap(amount_maxs[0], amount_maxs[1]);
		}
	}

	void PrecipitationSummarizer::append(uint16_t index, const SingleDatapoint& datapoint) {
		// Update the tally of precipitation types.
		amount.append(datapoint);

		// If a block of precipitation is active, continue appending into it.
		if (block_is_active) {
			block_is_active = blocks[block_index].append(index, datapoint.precipitation);
			if (!block_is_active)
				++block_index;
		}
		else if (Block::would_start_block(datapoint.precipitation)) {
			start_next_block(index);
			blocks[block_index].append(index, datapoint.precipitation);
		}
	}

	constexpr static inline int MINUTE_INDICES_PER_HOUR = 60/5;
	struct TimeSpec {
		bool hourly = false;
		bool ranged = false;
		uint16_t earliest{}, latest{};

		struct FromBegin {};
		struct FromEnd   {};

		TimeSpec() = default;
		TimeSpec(const PrecipitationSummarizer::Block& blk, FromBegin, bool hourly = false) : hourly(hourly) {
			if (blk.start == -2) {
				ranged = false;
				earliest = blk.left;
			}
			else if (blk.start - blk.left < 2) {
				ranged = false;
				earliest = blk.start;
			}
			else {
				ranged = true;
				earliest = blk.left;
				latest = blk.start;
			}
		}
		TimeSpec(const PrecipitationSummarizer::Block& blk, FromEnd, bool hourly = false) : hourly(hourly) {
			if (blk.end == -2 && blk.right != -1) {
				ranged = false;
				earliest = blk.right;
			}
			else if (blk.right == -1) {
				if (blk.end == -1) {
					ranged = false;
					earliest = hourly ? 35 : 72;
				}
				else {
					ranged = true;
					earliest = blk.end;
					latest = hourly ? 35 : 72;
				}
			}
			else if (blk.right - blk.end < 2) {
				ranged = false;
				earliest = blk.end;
			}
			else {
				ranged = true;
				earliest = blk.end;
				latest = blk.right;
			}
		}

		bool is_now() const { return earliest == 0 && !ranged; }

		enum Mode {
			FOR,               // for (~7 minutes, 5-25 minutes, 3 hours)
			UNTIL,             // starting/stopping, continuining/stopping <UNTIL_RELATIVE> (x minutes later, x hours later, this evening)
			IN,                // starting <IN>, stopping <IN> 
			LATER              // later <this morning>, <this evening> -- always uses LATER notation and never uses minutes.
		};

		enum RelativeHourDescription {
			THIS_MORNING = 0,
			THIS_AFTERNOON,
			THIS_EVENING,
			TONIGHT,
			TOMMOROW_MORNING,
			TOMMOROW_AFTERNOON,
			TOMMOROW_EVENING,
			TOMMOROW_NIGHT
		};

		int as_hours(uint16_t field) const {
			if (hourly)
				return field;
			else
				return field / MINUTE_INDICES_PER_HOUR;
		}

		int as_minutes(uint16_t field) const {
			if (!hourly)
				return field * 5;
			else
				return field * MINUTE_INDICES_PER_HOUR * 5;
		}

		int center() const {
			return ranged ? int(earliest + latest) / 2 : earliest;
		}

		bool worth_showing_as_ranged() const {
			if (hourly) {
				return ranged && (latest - earliest) > 2;
			}
			else {
				return ranged && (latest - earliest) > 3;
			}
		}

		RelativeHourDescription describe_as_vague_time() const {
			int center = this->center();
			if (!hourly) {
				center /= MINUTE_INDICES_PER_HOUR;
			}

			time_t t;
			struct tm stm;

			t = time(0);
			localtime_r(&t, &stm);

			if (stm.tm_hour + center > 23) {
				stm.tm_hour += (center - 24);
				if (stm.tm_hour < 4) {
					return TONIGHT; // use tonight for 2am this coming morning
				}
				else if (stm.tm_hour >= 22) {
					return TOMMOROW_NIGHT;
				}
				else if (stm.tm_hour < 12) {
					return TOMMOROW_MORNING;
				}
				else if (stm.tm_hour >= 12 && stm.tm_hour < 17) {
					return TOMMOROW_AFTERNOON;
				}
				else
					return TOMMOROW_EVENING;
			}
			else {
				stm.tm_hour += center;
				if (stm.tm_hour < 4 || stm.tm_hour >= 22) {
					return TONIGHT;
				}
				else if (stm.tm_hour >= 4 && stm.tm_hour < 12) {
					return THIS_MORNING;
				}
				else if (stm.tm_hour >= 12 && stm.tm_hour < 17) {
					return THIS_AFTERNOON;
				}
				else
					return THIS_EVENING;
			}
		}

		// Append to buf a string describing the current point in time (or if mode == FOR, the duration between this
		// time point and the previous one). All previous time points are examined to avoid saying something like "starting
		// tonight, continuing until tonight" and instead come up with "starting tonight, continuing until later tonight."
		//
		// Note that that could probably be better phrased as just "tonight", and so there's a method to detect consolidation
		// and just use the single time point description.
		int format_as(Mode m, const TimeSpec previous[], size_t previous_amt, char * buf, size_t buflen) const {
			static const char * const vague_descriptions[] = {
				"this morning",
				"this afternoon",
				"this evening",
				"tonight",
				"tomorrow morning",
				"tomorrow afternoon",
				"tomorrow evening",
				"tomorrow night"
			};

			const char * for_suffix = "", *for_prefix = "";
			int minute_diff = as_minutes(center());

			if (previous_amt) {
				minute_diff = as_minutes(center()) - previous[previous_amt - 1].as_minutes(previous[previous_amt - 1].center());
			}

			switch (m) {
				case FOR:
					{
						// For is special as it is always following the word " for ". For times >= 18 hours, we use "the day", for times >= 2 hours we use hours,
						// otherwise we use minutes.
						
						if (minute_diff > 18*60) {
							return snprintf(buf, buflen, "the day");
						}
						
					reuse_for:
						bool use_ranged = previous_amt > 0 ? previous[previous_amt - 1].worth_showing_as_ranged() && worth_showing_as_ranged() : worth_showing_as_ranged();
						
						if (minute_diff > 90 || hourly) {
							if (use_ranged) {
								int hr_a, hr_b;
								if (previous_amt) {
									hr_a = as_hours(earliest) - previous[previous_amt - 1].as_hours(previous[previous_amt - 1].latest);
									hr_b = as_hours(latest) - previous[previous_amt - 1].as_hours(previous[previous_amt - 1].earliest);
								}
								else {
									hr_a = as_hours(earliest);
									hr_b = as_hours(latest);
								}

								if (hr_a == hr_b || (hr_b < 4 && !hourly)) {
									int min_a = !previous_amt ? as_minutes(earliest) : (as_minutes(earliest) - previous[previous_amt - 1].as_minutes(previous[previous_amt - 1].latest));
									int min_b = !previous_amt ? as_minutes(latest) : (as_minutes(latest) - previous[previous_amt - 1].as_minutes(previous[previous_amt - 1].earliest));
									if (min_a == 0)
										return snprintf(buf, buflen, "%sup to %dh:%02dm%s", for_prefix, min_b / 60, min_b % 60, for_suffix);
									else
										return snprintf(buf, buflen, "%s%dh:%02dm-%dh:%02dm%s", for_prefix, min_a / 60, min_a % 60, min_b / 60, min_b % 60, for_suffix);
								}
								else {
									if (hr_a == 0) {
										if (hr_b == 1)
											return snprintf(buf, buflen, "%sup to an hour%s", for_prefix, for_suffix);
										else
											return snprintf(buf, buflen, "%sup to %d hours%s", for_prefix, hr_b, for_suffix);
									}
									else
										return snprintf(buf, buflen, "%s%d-%d hours%s", for_prefix, hr_a, hr_b, for_suffix);
								}
							}
							else if (minute_diff < 90)
								return snprintf(buf, buflen, "%san hour%s", for_prefix, for_suffix);
							else if (minute_diff >= 4*60 || hourly)
								return snprintf(buf, buflen, "%s~%d hours%s", for_prefix, (minute_diff + 30) / 60, for_suffix);
							else if (minute_diff % 60 == 0)
								return snprintf(buf, buflen, "%s%d hours%s", for_prefix, minute_diff / 60, for_suffix);
							else
								return snprintf(buf, buflen, "%s%dh:%02dm%s", for_prefix, minute_diff / 60, minute_diff % 60, for_suffix);
						}
						else {
							if (use_ranged) {
								if (previous_amt) {
									if (auto min_a = as_minutes(earliest) - previous[previous_amt - 1].as_minutes(previous[previous_amt - 1].latest); min_a != 0) {
										return snprintf(buf, buflen, "%s%d-%d minutes%s", for_prefix,
											min_a,
											as_minutes(latest) - previous[previous_amt - 1].as_minutes(previous[previous_amt - 1].earliest), for_suffix);
									}
									else {
										return snprintf(buf, buflen, "%sup to %d minutes%s", for_prefix,
											as_minutes(latest) - previous[previous_amt - 1].as_minutes(previous[previous_amt - 1].earliest), for_suffix);
									}
								}
								else if (auto min_a = as_minutes(earliest); min_a != 0) {
									return snprintf(buf, buflen, "%s%d-%d minutes%s", for_prefix, min_a, as_minutes(latest), for_suffix);
								}
								else {
									return snprintf(buf, buflen, "%sup to %d minutes%s", for_prefix, as_minutes(latest), for_suffix);
								}
							}
							else if (minute_diff <= 5)
								return snprintf(buf, buflen, "%s<5 minutes%s", for_prefix, for_suffix);
							else
								return snprintf(buf, buflen, "%s%d minutes%s", for_prefix, minute_diff, for_suffix);
						}
					}
					break;
				case IN:
					{
						// starting <IN>, stopping <IN>
						//   in x minutes, in x hours, this evening, tomorrow morning
						if (minute_diff < 5*60) {
							for_prefix = "in ";
							goto reuse_for;
						}
				common_use_descriptor:
						if (std::any_of(previous, previous + previous_amt, [&](const TimeSpec& x){return describe_as_vague_time() == x.describe_as_vague_time();}))
							goto later;
						else
							return snprintf(buf, buflen, "%s", vague_descriptions[describe_as_vague_time()]);
					}
					break;
				case UNTIL:
					{
						// until
						//   x minutes later, x hours later, this evening, tomorrow morning, later this evening, later this morning
						if (minute_diff < 5*60) {
							for_suffix = " later";
							goto reuse_for;
						}
						goto common_use_descriptor;
					}
					break;
				case LATER:
				later:
					return snprintf(buf, buflen, "later %s", vague_descriptions[describe_as_vague_time()]);
			}

			return 0;
		}

		// Returns true if the description of the time (in hourly mode) winds up as the same thing as a previous
		// item (to avoid saying "starting tonight, continuing until tonight")
		bool is_same_string_for(const TimeSpec previous[], size_t previous_amt) const {
			if (previous_amt == 0)
				return false;

			if (!previous[previous_amt - 1].will_use_vague_time(previous, previous_amt - 1))
				return false;

			return describe_as_vague_time() == previous[previous_amt - 1].describe_as_vague_time();
		}

		// Returns true if the description will use the vague descriptoins
		bool will_use_vague_time(const TimeSpec previous[], size_t previous_amt) const {
			int minute_diff = as_minutes(center());

			if (previous_amt)
				minute_diff = as_minutes(center()) - previous[previous_amt - 1].as_minutes(previous[previous_amt - 1].center());

			return minute_diff > 5*60;
		}
	};

	SummaryResult generate_summary(const PrecipitationSummarizer& minutely_summary, const PrecipitationSummarizer& hourly_summary, char * buf, size_t buflen) {
		// Check if any rain is present at all
		if (minutely_summary.empty() && hourly_summary.empty())
			return SummaryResult::Empty;

		enum TimeClassification {
			UNSET,

			CONTINUOUS, // <no parens:amountspec>.

			STARTING, // <amountspec> starting <future timepsec>
			STOPPING, // <amountspec> stopping <future timespec>
			
			SINGLE_FUTURE, // <amountspec> starting <future timespec>, stopping <relative timespec>
			SINGLE_FUTURE_RESTART, // <amountspec> stopping <future timespec>, continuing <relative timespec>

			DOUBLE_FUTURE, // <amountspec> starting <future timespec> for <duration timespec>, continuing <relative timespec>
			DOUBLE_FUTURE_RESTART, // <amountspec> stopping <future timespec>, continuing <relative timespec> for <duration timespec>
			
			// used for anything of the INTERMITTENT/CHUNKS* variety
			INTERMITTENT_STOPPING, // Intermittent/on-and-off <force parens:amountspec> <end relative timespec>
			STARTING_INTERMITTENT, // Intermittent/on-and-off <force parens:amountspec> starting <future timespec>, continuing <end relative timespec>

			SINGLE_THEN_INTERMITTENT, // <amountspec> starting <future timespec> for <duration timespec>, continuing intermittently/on-and-off <relative timespec>
			STOPPING_THEN_INTERMITTENT, // <amountspec> stopping <future timespec>, continuing intermittently/on-and-off <relative timespec>
		} time_classification = UNSET;

 		TimeSpec timespec_args[4]{}; // exact timestamps of the 4 events (start/stop) referred to by the above formats.

		bool use_possible_language[2]{}; // for each start-end pair in timespec_args, describe whether the underlying block
										 // termed it "possible only"

		bool chunks = false; // If set, intermittent mode becomes chunked mode (on-and-off)

		// Pick a time classification.
		//
		// There are a couple of general rules:
		// 	- Hourly data is almost never missing while minutely data is due to the overlap. Combined hourly/minutely
		// 	data is only processed if the minutely tracker is either unbounded on the right or within 10 minutes of
		// 	the start of the hourly data.

		enum FuseMode {
			NOT_FUSED,         // No fusing between minute/hour summaries
			FUSE_SKIP_FIRST,   // The first block in the hour summary is subsumed by the minutely data; treat minute.last() as ending at hour.last()
			FUSE_FROM_FIRST,   // Treat minutely.second() as ending at hour.first()

			TOTALLY_SUBSUMED,  		// The hourly summary is made entirely redundant by the minutely summary.
			FIRST_TOTALLY_SUBSUMED, // The hourly summary's first block is made entirely redundant.
		} fuse_mode = NOT_FUSED;
		
		// First, try to detect fusable situations
		if (!hourly_summary.empty() && !minutely_summary.empty()) {
			int16_t minutely_range_left = minutely_summary.blocks[0].left;
			int16_t minutely_range_right = minutely_summary.blocks[minutely_summary.last_filled_block_index()].right;

			auto before_range = [&](int hour) -> bool {
				if (hour == -1)
					return false;
				return hour * MINUTE_INDICES_PER_HOUR < minutely_range_left;
			};
			auto after_range = [&](int hour) -> bool {
				if (minutely_range_right == -1)
					return hour >= 6;
				else {
					if (hour == -1)
						return true;
					return hour * MINUTE_INDICES_PER_HOUR > minutely_range_right;
				}
			};

			if (minutely_range_right == -1 || minutely_range_right > 4 * MINUTE_INDICES_PER_HOUR) {
				if (!before_range(hourly_summary.blocks[0].right) && !after_range(hourly_summary.blocks[0].left)) {
					if (after_range(hourly_summary.blocks[0].right))
						fuse_mode = FUSE_FROM_FIRST;
					else
						fuse_mode = TOTALLY_SUBSUMED;
				}
				else
					fuse_mode = NOT_FUSED;

				if (hourly_summary.last_filled_block_index() == 1) {
					if (fuse_mode == FUSE_FROM_FIRST || fuse_mode == TOTALLY_SUBSUMED || fuse_mode == NOT_FUSED) {
						if (!before_range(hourly_summary.blocks[1].right) && !after_range(hourly_summary.blocks[1].left)) {
							if (after_range(hourly_summary.blocks[1].right))
								fuse_mode = FUSE_SKIP_FIRST;
							else
								fuse_mode = TOTALLY_SUBSUMED;
						}
						else if (fuse_mode == TOTALLY_SUBSUMED) {
							fuse_mode = FIRST_TOTALLY_SUBSUMED;
						}
					}
				}
			}
		}

		// Correct fuse mode if the hourly data is intermittent: we don't want to fuse into an intermittent region because
		// we'll lose the intermittent flag and end up saying something like "rain stopping in a day"
		if (fuse_mode > NOT_FUSED && fuse_mode < TOTALLY_SUBSUMED) {
			switch (hourly_summary.intermittent_flag) {
				case PrecipitationSummarizer::INTERMITTENT:
				case PrecipitationSummarizer::INTERMITTENT_THEN_CONTINUOUS:
				case PrecipitationSummarizer::CHUNKS_THEN_CONTINUOUS:
					switch (minutely_summary.intermittent_flag) {
						case weather::PrecipitationSummarizer::INTERMITTENT:
							if (hourly_summary.intermittent_flag == PrecipitationSummarizer::INTERMITTENT) break;
							[[fallthrough]];
						default:
							fuse_mode = NOT_FUSED;
							break;
						case weather::PrecipitationSummarizer::CONTINUOUS_THEN_CHUNKS:
						case weather::PrecipitationSummarizer::CONTINUOUS_THEN_INTERMITTENT:
							break;
					}
					break;
				default:
					break;
			}
		}

		// Now, decide on which events to actually use. We support a maximum of three blocks being rendered into text at any time, and the last
		// of those can only be taken from the unfused part of the hourly block. Work out where it is first.

		int16_t hourly_sidecar_index = -1;
		bool use_hourly_amountspec = false;

		switch (fuse_mode) {
			case NOT_FUSED:
				if (!hourly_summary.empty() && !minutely_summary.empty()) {
					if (minutely_summary.last_filled_block_index() == 0)
						hourly_sidecar_index = hourly_summary.last_filled_block_index();
					else
						hourly_sidecar_index = 0;
				}
				break;
			case FUSE_SKIP_FIRST:
			case TOTALLY_SUBSUMED:
				break;
			case FUSE_FROM_FIRST:
			case FIRST_TOTALLY_SUBSUMED:
				if (hourly_summary.last_filled_block_index() == 1)
					hourly_sidecar_index = 1;
				break;
		}

		// (note that as this point we haven't determined whether or not to use it -- if we don't the sidecar index is set back to -1).

		// Determine where the first two points are.
		if (!minutely_summary.empty()) {
			// At least one minutely block.
			timespec_args[0] = TimeSpec{minutely_summary.blocks[0], TimeSpec::FromBegin{}};
			use_possible_language[0] = !minutely_summary.blocks[0].is_likely();
			bool indeterminate_end = minutely_summary.blocks[0].right == -1;
			// Was the end of the block adjusted by fusing? Only consider this is the only event.
			if (minutely_summary.last_filled_block_index() == 0) {
				switch (fuse_mode) {
					default:
						if (indeterminate_end) 
							break;
						timespec_args[1] = TimeSpec{minutely_summary.blocks[0], TimeSpec::FromEnd{}};
						break;
					case FUSE_SKIP_FIRST:
						indeterminate_end = hourly_summary.blocks[1].right == -1;
						use_possible_language[0] &= !hourly_summary.blocks[1].is_likely();
						if (indeterminate_end)
							break;
						timespec_args[1] = TimeSpec{hourly_summary.blocks[1], TimeSpec::FromEnd{}, true};
						break;
					case FUSE_FROM_FIRST:
						indeterminate_end = hourly_summary.blocks[0].right == -1;
						use_possible_language[0] &= !hourly_summary.blocks[0].is_likely();
						if (indeterminate_end)
							break;
						timespec_args[1] = TimeSpec{hourly_summary.blocks[0], TimeSpec::FromEnd{}, true};
						break;
				}
			}
			else if (!indeterminate_end) {
				timespec_args[1] = TimeSpec{minutely_summary.blocks[0], TimeSpec::FromEnd{}};
			}

			// At this point we've recorded a single precipitation event; select the appropriate summary.
			if (indeterminate_end) {
				if (timespec_args[0].is_now())
					time_classification = CONTINUOUS;
				else
					time_classification = STARTING;
			}
			else {
				if (timespec_args[0].is_now())
					time_classification = STOPPING;
				else
					time_classification = SINGLE_FUTURE;
			}

			// If there are more minutely events, process them now:
			if (minutely_summary.last_filled_block_index() == 1) {
				// At least one minutely block.
				timespec_args[2] = TimeSpec{minutely_summary.blocks[1], TimeSpec::FromBegin{}};
				use_possible_language[1] = !minutely_summary.blocks[1].is_likely();
				indeterminate_end = minutely_summary.blocks[1].right == -1;
				// Was the end of the block adjusted by fusing?
				switch (fuse_mode) {
					default:
						timespec_args[3] = TimeSpec{minutely_summary.blocks[1], TimeSpec::FromEnd{}};
						break;
					case FUSE_SKIP_FIRST:
						indeterminate_end = hourly_summary.blocks[1].right == -1;
						use_possible_language[1] &= !hourly_summary.blocks[1].is_likely();
						timespec_args[3] = TimeSpec{hourly_summary.blocks[1], TimeSpec::FromEnd{}, true};
						break;
					case FUSE_FROM_FIRST:
						indeterminate_end = hourly_summary.blocks[0].right == -1;
						use_possible_language[1] &= !hourly_summary.blocks[0].is_likely();
						timespec_args[3] = TimeSpec{hourly_summary.blocks[0], TimeSpec::FromEnd{}, true};
						break;
				}

				// If the intermittent flag is set, process that instead
				if (minutely_summary.intermittent_flag) switch (minutely_summary.intermittent_flag) 
				{
					case PrecipitationSummarizer::CHUNKS_THEN_CONTINUOUS:
						chunks = true;
						[[fallthrough]];
					case PrecipitationSummarizer::INTERMITTENT:
					case PrecipitationSummarizer::INTERMITTENT_THEN_CONTINUOUS:
						if ((!indeterminate_end && !fuse_mode) || timespec_args[0].is_now()) {
							timespec_args[1] = timespec_args[3];
							time_classification = INTERMITTENT_STOPPING;
						}
						else {
							time_classification = STARTING_INTERMITTENT;
						}
						break;
					case PrecipitationSummarizer::CONTINUOUS_THEN_CHUNKS:
						chunks = true;
						[[fallthrough]];
					default:
						if (timespec_args[0].is_now())
							time_classification = STOPPING_THEN_INTERMITTENT;
						else
							time_classification = SINGLE_THEN_INTERMITTENT;
						break;
				}
				else if (indeterminate_end) {
					if (time_classification == SINGLE_FUTURE)
						time_classification = DOUBLE_FUTURE;
					else if (time_classification == STOPPING)
						time_classification = SINGLE_FUTURE_RESTART;
				}
				else {
					if (time_classification == SINGLE_FUTURE)
						time_classification = DOUBLE_FUTURE;
					else if (time_classification == STOPPING)
						time_classification = DOUBLE_FUTURE_RESTART;
				}
			}
		}
		// Only process a non-sidecar hourly summary if there is no minutely data.
		else if (!hourly_summary.empty()) {
			use_hourly_amountspec = true;

			switch (hourly_summary.intermittent_flag) {
				case PrecipitationSummarizer::CONTINUOUS:
					{
						int use_index = 0;

						// For hourly summaries, we're generally more concerned with the "big picture": an unlikely short burst of precipitation
						// is less interesting than a larger & later more likely burst. Prioritize as such.

						if (hourly_summary.last_filled_block_index() == 1 && !hourly_summary.blocks[0].is_likely() && hourly_summary.blocks[0].right - hourly_summary.blocks[0].left <= 2) {
							use_index = 1;
						}

						timespec_args[0] = TimeSpec{hourly_summary.blocks[use_index], TimeSpec::FromBegin{}, true};
						use_possible_language[0] = !hourly_summary.blocks[use_index].is_likely();
						bool indeterminate_end = hourly_summary.blocks[use_index].right == -1;
						if (!indeterminate_end) {
							timespec_args[1] = TimeSpec{hourly_summary.blocks[use_index], TimeSpec::FromEnd{}, true};
						}

						if (indeterminate_end) {
							if (timespec_args[0].is_now())
								time_classification = CONTINUOUS;
							else
								time_classification = STARTING;
						}
						else {
							if (timespec_args[0].is_now())
								time_classification = STOPPING;
							else
								time_classification = SINGLE_FUTURE;
						}
					}
					break;
				case PrecipitationSummarizer::CHUNKS_THEN_CONTINUOUS:
					chunks = true;
					[[fallthrough]];
				case PrecipitationSummarizer::INTERMITTENT_THEN_CONTINUOUS:
				case PrecipitationSummarizer::INTERMITTENT:
					{
						timespec_args[0] = TimeSpec{hourly_summary.blocks[0], TimeSpec::FromBegin{}, true};
						use_possible_language[0] = !hourly_summary.blocks[0].is_likely();
						bool indeterminate_end = hourly_summary.blocks[0].right == -1;
						if (!indeterminate_end) {
							timespec_args[1] = TimeSpec{hourly_summary.blocks[0], TimeSpec::FromEnd{}, true};
						}

						if (indeterminate_end || timespec_args[0].is_now()) {
							time_classification = INTERMITTENT_STOPPING;
						}
						else {
							time_classification = STARTING_INTERMITTENT;
						}
					}
					break;
				case PrecipitationSummarizer::CONTINUOUS_THEN_CHUNKS:
					chunks = true;
					[[fallthrough]];
				case PrecipitationSummarizer::CONTINUOUS_THEN_INTERMITTENT:
					{
						timespec_args[0] = TimeSpec{hourly_summary.blocks[0], TimeSpec::FromBegin{}, true};
						timespec_args[1] = TimeSpec{hourly_summary.blocks[0], TimeSpec::FromEnd{}, true};
						use_possible_language[0] = !hourly_summary.blocks[0].is_likely();
						timespec_args[2] = TimeSpec{hourly_summary.blocks[1], TimeSpec::FromBegin{}, true};
						use_possible_language[1] = !hourly_summary.blocks[1].is_likely();

						if (timespec_args[0].is_now())
							time_classification = STOPPING_THEN_INTERMITTENT;
						else
							time_classification = SINGLE_THEN_INTERMITTENT;
					}
					break;
				default:
					break;
			}
		}

		const auto& amount_spec = use_hourly_amountspec ? hourly_summary.amount : minutely_summary.amount;
		enum AmountSpecMode {
			NORMAL,
			NO_PAREN,
			FORCE_PAREN
		} amount_spec_mode = NORMAL;
		/* bool amount_spec_capital = amount_spec_mode == FORCE_PAREN */

		// Actually begin writing the summary:
		size_t remain = buflen;
		char * ptr = buf;

		// Updates remain+ptr
		auto append = [&](int len){
			remain -= len;
			ptr += len;
		};

		// Determine how to format the amount string.
		switch (time_classification) {
			case UNSET:
				ESP_LOGW(TAG, "unset time classification");
				return SummaryResult::Empty;
			case CONTINUOUS:
				amount_spec_mode = NO_PAREN;
				break;
			case INTERMITTENT_STOPPING:
			case STARTING_INTERMITTENT:
				amount_spec_mode = FORCE_PAREN;
				// If the amountspec is to be prefixed by the words "Intermittent", do so now.
				if (chunks) {
					append(snprintf(ptr, remain, use_possible_language[0] ? "Possible on-and-off " : "On-and-off "));
				}
				else {
					append(snprintf(ptr, remain, use_possible_language[0] ? "Possible intermittent " : "Intermittent "));
				}
				break;
			case STARTING:
			case STOPPING:
			case SINGLE_FUTURE:
			case SINGLE_FUTURE_RESTART:
			case DOUBLE_FUTURE:
			case DOUBLE_FUTURE_RESTART:
			case SINGLE_THEN_INTERMITTENT:
			case STOPPING_THEN_INTERMITTENT:
				break;
		}

		static const char * const condition_names[][4] = {
			/* uncapitalized, Capitalized, uncapitalized in thunderstorm, Capitalized in Thunderstorm */
			/* [NONE] = */ {},
			/* [RAIN] = */ {"rain", "Rain", "thunderstorms", "Thunderstorms"},
			/* [SNOW] = */ {"snow", "Snow", "thundersnow", "Thundersnow"},
			/* [FREEZING_RAIN] = */ {"freezing rain", "Freezing rain", "thundering freezing rain", "Thundering freezing rain"},
			/* [SLEET] = */ {"sleet", "Sleet", "thundersleet", "Thundersleet"},
		};

		// Begin formatting the amount string. The amount string does not get a space after it.
		//
		// There are two ways to format the amounts:
		// x (yy-zz mm/hr)
		// yy-zz mm/hr of x
		//
		// or just
		//
		// x
		//
		// Annotated amounts are only used if the amount is more than 8mm/hr (or for snow more than 5cm). Only
		// one parenthetical will occur, prioritizing rain.
		
		{
			bool capitalize = amount_spec_mode != FORCE_PAREN;
			// First, prefix the text with possible if required.
			if (use_possible_language[0] && capitalize) {
				append(snprintf(ptr, remain, "Possible "));
				capitalize = false;
			}

			// Then, handle mixed precipitation
			if (amount_spec.mixed_count * 10 / amount_spec.total_count > 3) {
				if (amount_spec.thunderstorms_present) {
					if (amount_spec.kinds[amount_spec.most_prevalent_index()] == slots::PrecipData::SNOW) {
						append(snprintf(ptr, remain, capitalize ? "Thundersnow" : "thundersnow"));
					}
					else {
						append(snprintf(ptr, remain, capitalize ? "Thunderstorms" : "thunderstorms"));
					}
				}
				else {
					append(snprintf(ptr, remain, capitalize ? "Mixed precipitation" : "mixed precipitation"));
				}
			}
			// Then, decide whether to quantify the weather amount
			else {
				int mpi = amount_spec.most_prevalent_index();
				bool quantifying = amount_spec.amount_maxs[mpi] > (amount_spec.kinds[mpi] == slots::PrecipData::SNOW ? 300 : 800);
				bool parens = amount_spec.kinds[1] == slots::PrecipData::NONE;
				if (amount_spec_mode == NO_PAREN) {
					quantifying = false;
					parens = false;
				}
				else if (amount_spec_mode == FORCE_PAREN) {
					parens = true;
				}
				bool quantify_range = amount_spec.kinds[mpi] != slots::PrecipData::SNOW && amount_spec.amount_maxs[mpi] - amount_spec.amount_mins[mpi] > 500 && amount_spec.amount_mins[mpi] > 100;

				static const char * const condition_units[] = {
					"",
					"mm/hr",
					"cm",
					"mm/hr",
					"mm/hr"
				};

				const char * unquantified_prefixes[2] = {"", ""};
				for (int idx = 0; idx < std::size(amount_spec.kinds); ++idx) {
					if (amount_spec.amount_maxs[idx] < 100) {
						unquantified_prefixes[idx] = (idx == 0 && capitalize) ? "Light " : "light ";
					}
					else if (amount_spec.amount_maxs[idx] > 500 && amount_spec.amount_maxs[idx] < 800) {
						unquantified_prefixes[idx] = (idx == 0 && capitalize) ? "Heavy " : "heavy ";
					}
				}

				// If only one type of weather is present, just present it directly.
				if (quantifying && mpi == 0) {
					if (parens) {
						if (quantify_range)
							append(snprintf(ptr, remain, "%s (%d-%d %s)", condition_names[amount_spec.kinds[0]][
								amount_spec.thunderstorms_present * 2 + capitalize], amount_spec.amount_mins[0] / 100, amount_spec.amount_maxs[0] / 100, condition_units[
								amount_spec.kinds[0]]));
						else
							append(snprintf(ptr, remain, "%s (%d %s)", condition_names[amount_spec.kinds[0]][
								amount_spec.thunderstorms_present * 2 + capitalize], amount_spec.amount_maxs[0] / 100, condition_units[
								amount_spec.kinds[0]]));
					}
					else {
						if (quantify_range)
							append(snprintf(ptr, remain, "%d-%d %s of %s", amount_spec.amount_mins[0] / 100, amount_spec.amount_maxs[0] / 100, condition_units[
								amount_spec.kinds[0]], condition_names[amount_spec.kinds[0]][
								amount_spec.thunderstorms_present * 2]));
						else
							append(snprintf(ptr, remain, "%d %s of %s", amount_spec.amount_maxs[0] / 100, condition_units[
								amount_spec.kinds[0]], condition_names[amount_spec.kinds[0]][
								amount_spec.thunderstorms_present * 2]));
					}
				}
				else {
					if (amount_spec.thunderstorms_present) {
						append(snprintf(ptr, remain, "%s", condition_names[amount_spec.kinds[0]][2 + capitalize]));
					}
					else {
						if (strlen(unquantified_prefixes[0]))
							capitalize = false;
						append(snprintf(ptr, remain, "%s%s", unquantified_prefixes[0], condition_names[amount_spec.kinds[0]][capitalize]));
					}
				}
				if (amount_spec.kinds[1] != slots::PrecipData::NONE) {
					if (quantifying && mpi == 1) {
						if (quantify_range)
							append(snprintf(ptr, remain, " and %d-%d %s of %s", amount_spec.amount_mins[1] / 100, amount_spec.amount_maxs[1] / 100, condition_units[
								amount_spec.kinds[1]], condition_names[amount_spec.kinds[1]][0]));
						else
							append(snprintf(ptr, remain, " and %d %s of %s", amount_spec.amount_maxs[1] / 100, condition_units[
								amount_spec.kinds[1]], condition_names[amount_spec.kinds[1]][0]));
					}
					else {
						append(snprintf(ptr, remain, " and %s%s", unquantified_prefixes[1], condition_names[amount_spec.kinds[1]][0]));
					}
				}
			}
		}

		SummaryResult result = SummaryResult::PartialSummary;

		// Finally, generate the entire main summary.
		switch (time_classification) {
			case UNSET:
				__builtin_unreachable();
			case CONTINUOUS:
			add_period:
				// Continuous just shows the condition string, but we do at least finish it with a period.
				append(snprintf(ptr, remain, "."));
				break;
			case STARTING:
				// <amountspec> starting <IN timespec>
				append(snprintf(ptr, remain, " starting "));
				append(timespec_args[0].format_as(TimeSpec::IN, timespec_args, 0, ptr, remain));
				goto add_period;
			case STOPPING:
			stopping:
				// <amountspec> stopping <IN timespec>
				//
				// note that args[0] == now, so use args[1]
				append(snprintf(ptr, remain, " stopping "));
				append(timespec_args[1].format_as(TimeSpec::IN, timespec_args, 1, ptr, remain));
				goto add_period;
			case SINGLE_FUTURE:
				// if timespecs are equivalent, condense to use "<amountspec> <future timespec>"
				if (timespec_args[1].is_same_string_for(timespec_args, 1)) {
					append(snprintf(ptr, remain, " "));
					append(timespec_args[0].format_as(TimeSpec::IN, timespec_args, 0, ptr, remain));
				}
				else {
					// <amountspec> starting <future timespec>, stopping <relative timespec>
					append(snprintf(ptr, remain, " starting "));
					append(timespec_args[0].format_as(TimeSpec::IN, timespec_args, 0, ptr, remain));
					append(snprintf(ptr, remain, ", stopping "));
					append(timespec_args[1].format_as(TimeSpec::UNTIL, timespec_args, 1, ptr, remain));
				}
				goto add_period;
			case SINGLE_FUTURE_RESTART:
				// if timespecs are equivalent, condense to use "<amountspec> stoppping <future timespec>"
				if (timespec_args[2].is_same_string_for(timespec_args + 1, 1))
					goto stopping;
				else {
					append(snprintf(ptr, remain, " stopping "));
					append(timespec_args[1].format_as(TimeSpec::IN, timespec_args, 1, ptr, remain));
					append(snprintf(ptr, remain, ", continuing "));
					append(timespec_args[2].format_as(TimeSpec::UNTIL, timespec_args, 2, ptr, remain));
				}
				hourly_sidecar_index = -1; // Disallow a sidecar.
				goto add_period;
			case DOUBLE_FUTURE:
				append(snprintf(ptr, remain, " starting "));
				append(timespec_args[0].format_as(TimeSpec::IN, timespec_args, 0, ptr, remain));
				append(snprintf(ptr, remain, " for "));
				append(timespec_args[1].format_as(TimeSpec::FOR, timespec_args, 1, ptr, remain));
				append(snprintf(ptr, remain, use_possible_language[1] && !use_possible_language[0] ? ", possibly continuing " : ", continuing "));
				append(timespec_args[2].format_as(TimeSpec::UNTIL, timespec_args, 2, ptr, remain));
				hourly_sidecar_index = -1; // Disallow a sidecar.
				result = SummaryResult::TotalSummary;
				goto add_period;
			case DOUBLE_FUTURE_RESTART:
				append(snprintf(ptr, remain, " stopping "));
				append(timespec_args[1].format_as(TimeSpec::IN, timespec_args, 1, ptr, remain));
				append(snprintf(ptr, remain, use_possible_language[1] && !use_possible_language[0] ? ", possibly continuing " : ", continuing "));
				append(timespec_args[2].format_as(TimeSpec::UNTIL, timespec_args, 2, ptr, remain));
				append(snprintf(ptr, remain, " for "));
				append(timespec_args[3].format_as(TimeSpec::FOR, timespec_args, 3, ptr, remain));
				hourly_sidecar_index = -1; // Disallow a sidecar.
				result = SummaryResult::TotalSummary;
				goto add_period;
			case INTERMITTENT_STOPPING:
				append(snprintf(ptr, remain, " stopping "));
				append(timespec_args[1].format_as(TimeSpec::IN, timespec_args, 1, ptr, remain));
				goto add_period;
			case STARTING_INTERMITTENT:
				append(snprintf(ptr, remain, " starting "));
				append(timespec_args[0].format_as(TimeSpec::IN, timespec_args, 0, ptr, remain));
				append(snprintf(ptr, remain, ", stopping "));
				append(timespec_args[1].format_as(TimeSpec::UNTIL, timespec_args, 1, ptr, remain));
				goto add_period;
			case SINGLE_THEN_INTERMITTENT:
				append(snprintf(ptr, remain, " starting "));
				append(timespec_args[0].format_as(TimeSpec::IN, timespec_args, 0, ptr, remain));
				append(snprintf(ptr, remain, " for "));
				append(timespec_args[1].format_as(TimeSpec::FOR, timespec_args, 1, ptr, remain));
				append(snprintf(ptr, remain, use_possible_language[1] && !use_possible_language[0] ? ", possibly continuing %s " : ", continuing %s ", chunks ? "on-and-off" : "intermittently"));
				append(timespec_args[2].format_as(TimeSpec::UNTIL, timespec_args, 2, ptr, remain));
				hourly_sidecar_index = -1;
				result = SummaryResult::TotalSummary;
				goto add_period;
				break;
			case STOPPING_THEN_INTERMITTENT:
				append(snprintf(ptr, remain, " stopping "));
				append(timespec_args[1].format_as(TimeSpec::IN, timespec_args, 1, ptr, remain));
				append(snprintf(ptr, remain, use_possible_language[1] && !use_possible_language[0] ? ", possibly continuing %s " : ", continuing %s ", chunks ? "on-and-off" : "intermittently"));
				append(timespec_args[2].format_as(TimeSpec::UNTIL, timespec_args, 2, ptr, remain));
				hourly_sidecar_index = -1;
				result = SummaryResult::TotalSummary;
				goto add_period;
				break;
		}

		if (hourly_sidecar_index != -1 && !use_hourly_amountspec && hourly_summary.blocks[hourly_sidecar_index].is_likely(1)) {
			result = SummaryResult::TotalSummary;

			// Determine which condition to sidecar
			append(snprintf(ptr, remain, " %s ", 
				condition_names[hourly_summary.amount.kinds[hourly_summary.amount.most_prevalent_index()]][hourly_summary.amount.thunderstorms_present * 2 + 1]));
			append(TimeSpec{hourly_summary.blocks[hourly_sidecar_index], TimeSpec::FromBegin{}, true}.format_as(TimeSpec::LATER, timespec_args, 0, ptr, remain));
			append(snprintf(ptr, remain, "."));
		}

		return result;
	}

	void HourlyConditionSummarizer::append(uint16_t index, const SingleDatapoint& datapoint) {
		// Check for strong wind:
		if (datapoint.wind_speed >= 4500 || datapoint.wind_gust >= 5500) {
			wind.latest = index;
			if (wind.earliest == -1) {
				wind.earliest = index;
			}
			else if (datapoint.wind_speed <= wind.wind_speed && datapoint.wind_gust <= wind.wind_gust_speed) {
				return;
			}

			wind.peak = index;
			wind.wind_speed = datapoint.wind_speed;
			wind.wind_gust_speed = datapoint.wind_gust;
		}

		// Check for clear & fog
		switch (datapoint.weather_code) {
			case slots::WeatherStateCode::CLEAR:
				if (clear.earliest == -1)
					clear.earliest = index;
				clear.count++;
				return;
			case slots::WeatherStateCode::LIGHT_FOG:
			case slots::WeatherStateCode::FOG:
				fog.latest = index;
				if (fog.earliest == -1)
					fog.earliest = index;
				if (fog.strongest_fog < datapoint.weather_code)
					fog.strongest_fog = datapoint.weather_code;
				return;
			default:
				return;
		}
	}

	HourlyConditionSummarizer::HourlySummaryType HourlyConditionSummarizer::current_summary_type(slots::WeatherStateCode current_code) const {
		// Wind is most important.
		if (is_present(wind)) {
			// Is the wind starting later?
			bool wind_later = starting_later(wind);

			if (wind.wind_speed > 7000 || wind.wind_gust_speed > 9000) {
				return wind_later ? EXTREMELY_WINDY_SOON : EXTREMELY_WINDY_STOPPING;
			}
			else
				return wind_later ? WINDY_SOON : WINDY_STOPPING;
		}

		// Otherwise if fog is present in the future, use that.
		if (is_present(fog) && fog.latest - fog.earliest + 1 >= 2) {
			return starting_later(fog) ? FOG_LATER : FOG_CLEARING_UP;
		}
		else switch (current_code) {
			case slots::WeatherStateCode::PARTLY_CLOUDY:
			case slots::WeatherStateCode::CLOUDY:
			case slots::WeatherStateCode::MOSTLY_CLOUDY:
				if (starting_later(clear) && clear.count > 2)
					return CLOUDS_CLEARING;
				[[fallthrough]];
			default:
				return CURRENT_CONDITIONS;
		}
	}

	void HourlyConditionSummarizer::generate_summary(slots::WeatherStateCode current_code, SummaryResult prior_result, char *buf, size_t buflen) const {
		size_t remain = buflen;
		char * ptr = buf;

		// Updates remain+ptr
		auto append = [&](int len){
			remain -= len;
			ptr += len;
		};

		static const char * const fog_titles[2] = {
			"Light fog",
			"Fog"
		};

		switch (auto code = current_summary_type(current_code)) {
			case WINDY_SOON:
			case WINDY_STOPPING:
				{
					TimeSpec timespec{};
					timespec.earliest = code == WINDY_SOON ? wind.earliest : wind.latest;
					timespec.hourly = true;

					append(snprintf(ptr, remain, code == WINDY_SOON ? "Very windy " : "Very windy conditions "));
					if (prior_result == SummaryResult::Empty) {
						append(snprintf(ptr, remain, "(up to %d km/h) ", wind.wind_speed / 100));
					}
					if (code == WINDY_STOPPING) {
						if (timespec.earliest > 24) {
							append(snprintf(ptr, remain, "for the day"));
							break;
						}
						append(snprintf(ptr, remain, "stopping "));
					}
					append(timespec.format_as(TimeSpec::IN, &timespec, 0, ptr, remain));
				}
				break;
			case EXTREMELY_WINDY_SOON:
				{
					TimeSpec starting_at{}, peaking_at{};
					starting_at.earliest = wind.earliest;
					starting_at.hourly = true;
					peaking_at.earliest = wind.peak;
					peaking_at.hourly = true;

					append(snprintf(ptr, remain, "Dangerously windy "));
					append(starting_at.format_as(TimeSpec::IN, &starting_at, 0, ptr, remain));
					if (prior_result == SummaryResult::Empty) {
						if (peaking_at.is_same_string_for(&starting_at, 1)) {
							append(snprintf(ptr, remain, " (up to %d km/h with gusts of %d km/h)", wind.wind_speed, wind.wind_gust_speed));
						}
						else {
							append(snprintf(ptr, remain, ", peaking "));
							append(peaking_at.format_as(TimeSpec::UNTIL, &starting_at, 1, ptr, remain));
							append(snprintf(ptr, remain, " at %d km/h (with gusts of up to %d km/h)", wind.wind_speed, wind.wind_gust_speed));
						}
					}
					else {
						append(snprintf(ptr, remain, " (up to %d km/h)", wind.wind_speed));
					}
				}
				break;
			case EXTREMELY_WINDY_STOPPING:
				{
					TimeSpec stopping_at{};
					stopping_at.earliest = wind.latest;
					stopping_at.hourly = true;

					append(snprintf(ptr, remain, "Dangerously windy conditions (up to %d km/h with gusts of %d km/h) ", wind.wind_speed, wind.wind_gust_speed));
					if (stopping_at.earliest > 24) {
						append(snprintf(ptr, remain, "for the day"));
					}
					else {
						append(snprintf(ptr, remain, "stopping "));
						append(stopping_at.format_as(TimeSpec::IN, &stopping_at, 0, ptr, remain));
					}
				}
				break;
			case FOG_CLEARING_UP:
				{
					TimeSpec stopping_at{};
					stopping_at.earliest = fog.latest;
					stopping_at.hourly = true;

					if (stopping_at.earliest > 24)
						goto current_conditions;	

					append(snprintf(ptr, remain, "%s clearing up ", fog_titles[fog.strongest_fog == slots::WeatherStateCode::FOG]));
					append(stopping_at.format_as(TimeSpec::IN, &stopping_at, 0, ptr, remain));
				}
				break;
			case FOG_LATER:
				{
					TimeSpec starting_at{};
					starting_at.earliest = fog.earliest;
					starting_at.hourly = true;

					if (starting_at.earliest > 24)
						goto current_conditions;	

					append(snprintf(ptr, remain, "%s ", fog_titles[fog.strongest_fog == slots::WeatherStateCode::FOG]));
					append(starting_at.format_as(TimeSpec::IN, &starting_at, 0, ptr, remain));
				}
				break;
			case CLOUDS_CLEARING:
			case CURRENT_CONDITIONS:
			current_conditions:
				// First, print the current condition title.
				{
					const char *title = "Unknown weather";
					switch (current_code) {
						case slots::WeatherStateCode::CLEAR:
							title = "Clear"; break;
						case slots::WeatherStateCode::PARTLY_CLOUDY:
							title = "Partly cloudy"; break;
						case slots::WeatherStateCode::CLOUDY:
							title = "Cloudy"; break;
						case slots::WeatherStateCode::MOSTLY_CLOUDY:
							title = "Mostly cloudy"; break;
						case slots::WeatherStateCode::DRIZZLE:
							title = "Drizzle"; break;
						case slots::WeatherStateCode::LIGHT_RAIN:
							title = "Light rain"; break;
						case slots::WeatherStateCode::RAIN:
							title = "Rain"; break;
						case slots::WeatherStateCode::HEAVY_RAIN:
							title = "Heavy rain"; break;
						case slots::WeatherStateCode::SNOW:
							title = "Snow"; break;
						case slots::WeatherStateCode::FLURRIES:
							title = "Flurries"; break;
						case slots::WeatherStateCode::LIGHT_SNOW:
							title = "Light snow"; break;
						case slots::WeatherStateCode::HEAVY_SNOW:
							title = "Heavy snow"; break;
						case slots::WeatherStateCode::FREEZING_DRIZZLE:
							title = "Freezing drizzle"; break;
						case slots::WeatherStateCode::FREEZING_LIGHT_RAIN:
							title = "Freezing light rain"; break;
						case slots::WeatherStateCode::FREEZING_RAIN:
							title = "Freezing rain"; break;
						case slots::WeatherStateCode::FREEZING_HEAVY_RAIN:
							title = "Freezing heavy rain"; break;
						case slots::WeatherStateCode::LIGHT_FOG:
							title = "Lightly foggy"; break;
						case slots::WeatherStateCode::FOG:
							title = "Foggy"; break;
						case slots::WeatherStateCode::LIGHT_ICE_PELLETS:
							title = "Light ice pellets"; break;
						case slots::WeatherStateCode::ICE_PELLETS:
							title = "Ice pellets"; break;
						case slots::WeatherStateCode::HEAVY_ICE_PELLETS:
							title = "Heavy ice pellets"; break;
						case slots::WeatherStateCode::THUNDERSTORM:
							title = "Thunderstorms"; break;
						case slots::WeatherStateCode::WINDY:
							title = "Windy"; break;
						default:
							break;
					}

					if (prior_result != SummaryResult::Empty) {
						int len = snprintf(ptr, remain, "Currently %s", title);
						if (len > 10) {
							ptr[10] = tolower(ptr[10]);
						}
						append(len);
					}
					else {
						append(snprintf(ptr, remain, "%s", title));
					}

					if (code == CLOUDS_CLEARING && clear.earliest < 24) {
						TimeSpec starting_at{};
						starting_at.earliest = clear.earliest;
						starting_at.hourly = true;

						append(snprintf(ptr, remain, ", clearing up "));
						append(starting_at.format_as(TimeSpec::IN, &starting_at, 0, ptr, remain));
					}
				}
				break;
		}

		if (*ptr == ')') {
			*ptr = '.';
			append(snprintf(ptr, remain, ")"));
		}
		else if (*ptr == ' ') {
			*ptr = '.';
		}
		else {
			append(snprintf(ptr, remain, "."));
		}
	}
}
