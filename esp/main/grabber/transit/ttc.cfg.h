#include <stddef.h>
#include <stdint.h>
#include "../../config.h"
#include "./common.cfg.h"

namespace transit::ttc {
	//!cfg: holds .ttc.alert_search
	extern const char * alert_search;

	//!cfg: holds .ttc.agency_code, default "ttc"
	extern const char * agency_code;

	//!cfg: receives .ttc.entries[$n].name
	void update_ttc_entry_name(size_t n, const char * value);
	
	struct TTCEntry {
		//!cfg: holds .dirtag, default nullptr
		const char * dirtag[4];
		//!cfg: holds .stopid
		int stopid = -1;
		//!cfg: holds .dirtag_alt, default nullptr
		const char * alt_dirtag[4];
		//!cfg: holds .stopid_alt
		int alt_stopid = -1;

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
