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
		TTC_TIME_3 = 0x26,

		WEATHER_ICON = 0x40,
		WEATHER_INFO = 0x44,
		WEATHER_STATUS = 0x45,

		CALFIX_INFO = 0x50,
		CALFIX_CLS1 = 0x51,
		CALFIX_CLS2 = 0x52,
		CALFIX_CLS3 = 0x53,
		CALFIX_CLS4 = 0x54,
	};

#pragma pack (push, 1)
	struct TTCTime {
		uint64_t tA, tB;
	};

	struct TTCInfo {
		uint16_t flags;
		uint8_t nameLen[3];

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

	struct WeatherInfo {
		float ctemp;
		float ltemp;
		float htemp;
		uint8_t status_size;
	};

	struct VStr {
		uint8_t index;
		uint8_t size;
		uint8_t data[14];
	};

	struct CalfixInfo {
		uint8_t day;
		uint8_t sched;
	};

#pragma pack (pop)

	static_assert(sizeof(TTCTime) <= 16, "TTC time too large");
	static_assert(sizeof(TTCInfo) <= 16, "TTCInfo too large");
	static_assert(sizeof(WeatherInfo) <= 16, "WeatherInfo too large");
	static_assert(sizeof(VStr) <= 16, "VStr too large");
	static_assert(sizeof(CalfixInfo) <= 16, "CalfixInfo too large");
}

#endif
