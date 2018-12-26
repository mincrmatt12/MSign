#ifndef SLOTS_H
#define SLOTS_H

#include <stdint.h>

namespace slots {
	enum DataID : uint16_t {
		WIFI_STATUS = 0x01,			// BOOL; 0 - disconnected, 1 - connected; size 1

		TIME_OF_DAY = 0x10, 		// UINT64_T; unix timestamp in milliseconds; size 4; polled

		TTC_INFO = 0x20,
		TTC_NAME_1 = 0x21,
		TTC_NAME_2 = 0x22,
		TTC_NAME_3 = 0x23,
		TTC_TIME_1 = 0x24,
		TTC_TIME_2 = 0x25,
		TTC_TIME_3 = 0x26
	};

#pragma pack (push, 1)
	struct TTCTime {
		uint64_t tA, tB;
	};

	struct TTCInfo {
		uint16_t flags;
		uint8_t nameLenA;
		uint8_t nameLenB;

		enum Flag : uint16_t {
			ALERT_0 = 1,
			ALERT_1 = 2,  // there is an alert
			ALERT_2 = 4,

			EXIST_0 = 8,
			EXIST_1 = 16,  // the slot has data r.n.
			EXIST_2 = 32,

			DELAY_0 = 64,
			DELAY_1 = 128, // the slot is reported as delayed
			DELAY_2 = 256
		};
	};
#pragma pack (pop)

	static_assert(sizeof(TTCTime) <= 16, "TTC time too large");
	static_assert(sizeof(TTCInfo) <= 16, "TTCInfo too large");
}

#endif
