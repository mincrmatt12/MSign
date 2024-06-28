#pragma once

#include "../config.h"

namespace parcels {
	//!cfg: receives .parcels.trackers[$i].name
	void set_parcel_name(size_t i, const char* value);
	
	struct ParcelConfig {
		//!cfg: holds .carrier_id
		mutable int carrier_id {};

		//!cfg: holds .final_carrier_id
		mutable int final_carrier_id {};

		//!cfg: holds .tracking_number
		config::string_t tracking_number {};

		//!cfg: holds .additional_param
		config::string_t additional_param {};

		//!cfg: holds .translate_messages
		bool translate_messages = true;

		//!cfg: holds .raw_timestamp
		bool raw_timestamp = false;
	};

	//!cfg: holds .parcels.trackers!size, default 0
	extern uint8_t parcel_count;

	//!cfg: holds .parcels.key, default nullptr
	extern config::string_t parcels_api_key;

	//!cfg: holds .parcels.trackers
	extern ParcelConfig tracker_configs[6];
}
