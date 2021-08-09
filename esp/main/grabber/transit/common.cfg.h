namespace transit {
	enum TransitImplementation {
		NONE,
		TTC,
		GTFS
	};

	//!cfg: holds .transit_implementation, default transit::NONE
	extern TransitImplementation impl;
}
