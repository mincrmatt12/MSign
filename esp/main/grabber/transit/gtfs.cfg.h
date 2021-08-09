#include "../../config.h"

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
	};

	//!cfg: holds .gtfs.entries
	extern GtfsEntry entries[3];
}
