#include "../../config.h"
#include "./common.cfg.h"

namespace transit::gtfs {
	//!cfg: holds .gtfs.feed.host, default "_webapps.regionofwaterloo.ca"
	extern config::string_t feed_host;

	//!cfg: holds .gtfs.feed.url, default "/api/grt-routes/api/tripupdates"
	extern config::string_t feed_url;

	//!cfg: holds .gtfs.alt_feed.host, default nullptr
	extern config::string_t alt_feed_host;

	//!cfg: holds .gtfs.alt_feed.url, default nullptr
	extern config::string_t alt_feed_url;

	//!cfg: receives .gtfs.entries[$n].name
	void update_ttc_entry_name(size_t n, const char * value);

	struct GtfsEntry {
		//!cfg: holds .route
		config::string_t route_id;

		//!cfg: holds .stop
		config::string_t stop_id;

		//!cfg: holds .route_alt
		config::string_t route_id_alt{};

		//!cfg: holds .stop_alt
		config::string_t stop_id_alt{};

		//!cfg: holds .use_alt_feed
		bool use_alt_feed = false;

		//!cfg: holds .dircode[0]
		TransitDirectionCode dir_a = NA;

		//!cfg: holds .dircode[1]
		TransitDirectionCode dir_b = NA;

		//!cfg: holds .distance
		uint8_t distance_minutes = 5;

		operator bool() const {return route_id && stop_id;}

		bool has_alt() const {return route_id_alt && stop_id_alt;}
	};

	//!cfg: holds .gtfs.entries
	extern config::lazy_t<GtfsEntry> entries[5];
}
