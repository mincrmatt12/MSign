#pragma once

#include "../config.h"

namespace parcels {

	//!cfg: receives .parcels.trackers[$i].name
	void set_parcel_name(size_t i, const char* value);
	
	struct ParcelConfig {
		//!cfg: holds .enabled
		mutable bool enabled = false; // set to false when detected as nonexistent

		//!cfg: holds .tracker_id
		config::string_t tracker_id {};
	};

	//!cfg: holds .parcels.key, default nullptr
	extern config::string_t parcels_api_key;

	//!cfg: holds .parcels.trackers
	extern ParcelConfig tracker_configs[6];
}
