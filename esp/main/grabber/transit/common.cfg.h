#pragma once

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
}
