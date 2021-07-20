#include <stddef.h>
#include <stdint.h>
#include "../config.h"

namespace ttc {
	//!cfg: holds .ttc.alert_search
	extern const char * alert_search;

	//!cfg: receives .ttc.entries[$n].name
	void update_ttc_entry_name(size_t n, const char * value);
	
	struct TTCEntry {
		//!cfg: holds .dirtag, default nullptr
		const char * dirtag[4];
		//!cfg: holds .stopid
		int stopid = -1;

		operator bool() const {return stopid != -1;}
	};

	//!cfg: holds .ttc.entries
	extern config::lazy_t<TTCEntry> entries[3];
}
