#include "../config.h"
#include "../common/slots.h"
#include <stdint.h>

namespace sccfg {
	const inline uint8_t number_of_screens = 4;

	struct ScreenEntry {
		struct OnOffTime {
			//!cfg: holds .start
			int32_t start = 0;

			//!cfg: holds .end
			int32_t end = -1;

			bool always_on() const {return end == -1;}
			bool on_for(unsigned int curtime) {
				if (start < end) return (curtime >= start) && (curtime <= end);
				else             return (curtime >= end) || (curtime <= start);
			}
		};

		//!cfg: holds .screen, default slots::ScCfgTime::ScreenId::TTC
		slots::ScCfgTime::ScreenId screen;

		//!cfg: holds .length
		int32_t duration = 10000;

		//!cfg: holds .times
		OnOffTime times{};
	};

	//!cfg: holds .sccfg.screens
	extern config::lazy_t<ScreenEntry> screen_entries[8];
}
