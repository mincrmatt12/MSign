#include "../../config.h"
#include "./common.cfg.h"

namespace transit::gtfs {
	//!cfg: holds .gtfs.feed.host, default "_webapps.regionofwaterloo.ca"
	extern const char * feed_host;

	//!cfg: holds .gtfs.feed.url, default "/api/grt-routes/api/tripupdates"
	extern const char * feed_url;

	//!cfg: receives .gtfs.entries[$n].name
	void update_ttc_entry_name(size_t n, const char * value);

	struct GtfsEntry {
		//!cfg: holds .route
		const char * route_id;

		//!cfg: holds .stop
		const char * stop_id;

		//!cfg: holds .route_alt
		const char * route_id_alt;

		//!cfg: holds .stop_alt
		const char * stop_id_alt;

		//!cfg: holds .dircode[0]
		TransitDirectionCode dir_a = NA;

		//!cfg: holds .dircode[1]
		TransitDirectionCode dir_b = NA;

		operator bool() const {return route_id && stop_id;}

		bool has_alt() const {return route_id_alt && stop_id_alt;}
	};

	//!cfg: holds .gtfs.entries
	extern config::lazy_t<GtfsEntry> entries[5];
}
