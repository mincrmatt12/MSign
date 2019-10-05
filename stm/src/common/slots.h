#ifndef SLOTS_H
#define SLOTS_H

#include <stdint.h>

namespace slots {
	// The comments here are important, as they get parsed by the dissector.
	// They _must_ be in the format
	// <type>; [optionalparameter] (stuff)
	// or
	// ''
	// to indicate the same as previous
	// the structs with flags must be in the form uint<>_t something_bla, enum SomethingBla
	enum DataID : uint16_t {
		WIFI_STATUS = 0x01,			// BOOL; 0 - disconnected, 1 - connected
		TIME_STATUS = 0x02,			// BOOL; 0 - waiting for NTP, 1 - NTP in sync

		TIME_OF_DAY = 0x10, 		// UINT64_T; unix timestamp in milliseconds; polled
		REQUEST_SCREEN = 0x11, 		// UINT8_T; screen ID to request, sent very infrequently
		VIRTUAL_BUTTONMAP = 0x12, 	// UINT8_T; bitmap, override of the data on GPIOA for the buttons

		TTC_INFO = 0x20,			// STRUCT; TTCInfo
		TTC_NAME_1 = 0x21,			// STRING; name of bus/tram in slot 1; polled
		TTC_NAME_2 = 0x22,			// ''
		TTC_NAME_3 = 0x23,			// ''
		TTC_TIME_1 = 0x24,			// STRUCT; TTCTime - time of next two buses
		TTC_TIME_2 = 0x25,			// ''
		TTC_TIME_3 = 0x26,			// ''
		TTC_TIME_1B = 0x27,			// STRUCT; TTCTime - time of next two buses after previous (3 and 4)
		TTC_TIME_2B = 0x28,			// ''
		TTC_TIME_3B = 0x29,			// ''

		WEATHER_ICON = 0x40,		// STRING; icon name from darksky
		WEATHER_INFO = 0x44,		// STRUCT; WeatherInfo
		WEATHER_STATUS = 0x45,		// STRUCT; VStr - current summary; polled
		WEATHER_ARRAY1 = 0x46,      // STRING; list of ENUMS for the state per-hour
		WEATHER_ARRAY2 = 0x47,      // '' 
		WEATHER_TEMP_GRAPH = 0x41,  // STRUCT; VStr - temp data as floats

		CALFIX_INFO = 0x50,			// STRUCT; CalfixInfo, current school day and schedule as int
		CALFIX_CLS1 = 0x51,			// STRUCT; ClassInfo, name and room
		CALFIX_CLS2 = 0x52,			// ''
		CALFIX_CLS3 = 0x53,			// ''
		CALFIX_CLS4 = 0x54,			// ''
		CALFIX_PRDH1 = 0x55, 		// STRUCT; PeriodInfo, start time for period 1 & 2
		CALFIX_PRDH2 = 0x56, 		// '', 3 & 4

		MODEL_INFO = 0x0900, 		// UINT16_T; number of triangles in the model
		MODEL_XYZ1 = 0x0901, 		// STRUCT; Vec3; position of point 1; requested per triangle
		MODEL_XYZ2 = 0x0902, 		// ''
		MODEL_XYZ3 = 0x0903, 		// ''
		MODEL_RGB  = 0x0904, 		// '', color of triangle
		MODEL_CAM_MINPOS  = 0x9005, // STRUCT; Vec3; minimum position of the camera 
		MODEL_CAM_MAXPOS  = 0x9006, // ''; maximum position of the camera 
		MODEL_CAM_FOCUS   = 0x9007, // ''; lookat point of camera
		MODEL_CAM_POS_OVR = 0x9010, // ''; overriden position of camera       |
		MODEL_CAM_TGT_OVR = 0x9011, // ''; overriden lookat point of camera   |> if these three options are all zero use random camera
		MODEL_CAM_UP_OVR  = 0x9012, // ''; overriden up vector of camera      |

		SCCFG_INFO = 0xb0, 			// STRUCT; ScCfgInfo, enabled screen bitmask, screen on/off
		SCCFG_TIMING = 0xb1,     	// STRUCT; ScCfgTime, how long to enable a certain screen; polled (inited to zero when SCCFG_INFO is updated, circular)
	};

#pragma pack (push, 1)
	struct TTCTime {
		uint64_t tA, tB;
	};

	struct TTCInfo {
		uint16_t flags;
		uint8_t nameLen[3];

		enum Flags : uint16_t {
			ALERT_0 = 1,
			ALERT_1 = 2,  // there is an alert
			ALERT_2 = 4,

			EXIST_0 = 8,
			EXIST_1 = 16,  // the slot has data r.n.
			EXIST_2 = 32,

			DELAY_0 = 64,
			DELAY_1 = 128, // the slot is reported as delayed
			DELAY_2 = 256,
		};
	};

	struct WeatherInfo {
		float ctemp;
		float ltemp;
		float htemp;
		float crtemp;
	};

	struct VStr {
		uint8_t index;
		uint8_t size;
		uint8_t data[14];
	};

	struct CalfixInfo {
		uint8_t day;
		bool active;
		bool abnormal;
	};

	struct ClassInfo {
		uint8_t name[11];
		uint16_t room;
	};

	struct PeriodInfo {
		uint64_t ps1, ps2;
	};

	struct ScCfgInfo {
		uint16_t enabled_mask;
		bool display_on;

		enum EnabledMask : uint16_t {
			TTC = 1,
			WEATHER = 2,
			MODEL = 4,
			PERIODS = 8,
			PARCELS = 16
		};
	};

	struct ScCfgTime {
		uint32_t millis_enabled;
		uint8_t idx_order;
	};

	struct Vec3 {
		float x;
		float y;
		float z;
	};

#pragma pack (pop)

#define SLOT_CHECK_DEF(sn)	static_assert(sizeof(sn) <= 16, #sn " too large")

	SLOT_CHECK_DEF(TTCTime);
	SLOT_CHECK_DEF(TTCInfo);
	SLOT_CHECK_DEF(WeatherInfo);
	SLOT_CHECK_DEF(VStr);
	SLOT_CHECK_DEF(CalfixInfo);
	SLOT_CHECK_DEF(ClassInfo);
	SLOT_CHECK_DEF(PeriodInfo);
	SLOT_CHECK_DEF(ScCfgInfo);
	SLOT_CHECK_DEF(ScCfgTime);
	SLOT_CHECK_DEF(Vec3);

	enum struct WeatherStateArrayCode : uint8_t {
		UNK = 0,

		CLEAR = 0x10,
		PARTLY_CLOUDY,
		MOSTLY_CLOUDY,
		OVERCAST,

		DRIZZLE = 0x20,
		LIGHT_RAIN,
		RAIN,
		HEAVY_RAIN,

		SNOW = 0x30,
		HEAVY_SNOW
	};


}

#endif
