#include <stddef.h>
#include <stdint.h>
#include "../../config.h"
#include "./common.cfg.h"

namespace transit::ttc {
	//!cfg: holds .ttc.alert_search
	extern config::string_t alert_search;

	//!cfg: holds .ttc.agency_code, default "ttc"
	extern config::string_t agency_code;
	
	struct TTCEntry {
		//!cfg: holds .dirtag
		config::string_t dirtag[4]{};
		//!cfg: holds .stopid
		int stopid = -1;
		//!cfg: holds .dirtag_alt
		config::string_t alt_dirtag[4]{};
		//!cfg: holds .stopid_alt
		int alt_stopid = -1;
		//!cfg: holds .distance
		uint8_t distance_minutes = 5;

		operator bool() const {return stopid != -1;}

		bool has_alt() {return alt_stopid != -1;}

		//!cfg: holds .dircode[0]
		TransitDirectionCode dir_a = NA;

		//!cfg: holds .dircode[1]
		TransitDirectionCode dir_b = NA;
	};

	//!cfg: holds .ttc.entries
	extern config::lazy_t<TTCEntry> entries[5];
}
