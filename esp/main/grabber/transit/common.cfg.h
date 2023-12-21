#pragma once

#include "../../config.h"

namespace transit {
	enum TransitImplementation {
		NONE,
		TTC,
		GTFS
	};

	//!cfg: holds .transit_implementation, default transit::TTC
	extern TransitImplementation impl;

	enum TransitDirectionCode : char {
		NA = '\x00',

		WEST = 'W',
		EAST = 'E',
		NORTH = 'N',
		SOUTH = 'S'
	};

	struct BaseEntryStaticInfo {
		//!cfg: holds .name
		config::string_t name;

		void load(size_t i);
	};

	struct BaseEntryStaticInfos {
		//!cfg: holds .entries
		BaseEntryStaticInfo entries[5];

		void load();
	};

	//!cfg: loads .$impl
	bool load_name_list(const char * impl, BaseEntryStaticInfos& value);

	void load_static_info(const char * impl);
}
