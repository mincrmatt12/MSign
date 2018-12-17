#ifndef SLOTS_H
#define SLOTS_H

#include <stdint.h>

namespace slots {
	enum DataID : uint16_t {
		WIFI_STATUS = 0x01,			// BOOL; 0 - disconnected, 1 - connected; size 1

		TIME_OF_DAY = 0x10, 		// UINT64_T; unix timestamp in milliseconds; size 4; polled
	};


}

#endif
