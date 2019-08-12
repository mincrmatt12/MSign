#ifndef SLOTS_H
#define SLOTS_H

#include <stdint.h>

namespace slots {
	enum DataID : uint16_t {
		WIFI_STATUS = 0x01,			// BOOL; 0 - disconnected, 1 - connected
		TIME_STATUS = 0x02,			// BOOL; 0 - waiting for NTP, 1 - NTP in sync

		UPDATE_STATE = 0x1a01,		// UINT8_T; enum - see below
		UPDATE_PCENT_DOWN = 0x1a02, // UINT8_t; percent downloaded of the update package 

		TIME_OF_DAY = 0x10, 		// UINT64_T; unix timestamp in milliseconds; polled
		REQUEST_SCREEN = 0x11, 		// UINT8_T; screen ID to request, sent very infrequently
		VIRTUAL_BUTTONMAP = 0x12, 	// UINT8_t; bitmap, override of the data on GPIOA for the buttons

		TTC_INFO = 0x20,			// STRUCT; TTCInfo
		TTC_NAME_1 = 0x21,			// VSTR; name of bus/tram in slot 1; polled
		TTC_NAME_2 = 0x22,			// ''
		TTC_NAME_3 = 0x23,			// ''
		TTC_TIME_1 = 0x24,			// STRUCT; TTCTime - time of next two buses
		TTC_TIME_2 = 0x25,			// ''
		TTC_TIME_3 = 0x26,			// ''

		WEATHER_ICON = 0x40,		// STRING; icon name from darksky
		WEATHER_INFO = 0x44,		// STRUCT; WeatherInfo
		WEATHER_STATUS = 0x45,		// VSTR; current summary; polled

		CALFIX_INFO = 0x50,			// STRUCT; CalfixInfo, current school day and schedule as int
		CALFIX_CLS1 = 0x51,			// STRUCT; ClassInfo, name and room
		CALFIX_CLS2 = 0x52,			// ''
		CALFIX_CLS3 = 0x53,			// ''
		CALFIX_CLS4 = 0x54,			// ''
		CALFIX_PRDH1 = 0x55, 		// STRUCT; PeriodInfo, start time for period 1 & 2
		CALFIX_PRDH2 = 0x56, 		// '', 3 & 4

		MODEL_INFO = 0x0900, 		// UINT16_t; number of triangles in the model
		MODEL_XYZ1 = 0x0901, 		// STRUCT; Vec3; position of point 1; requested per triangle
		MODEL_XYZ2 = 0x0902, 		// ''
		MODEL_XYZ3 = 0x0903, 		// ''
		MODEL_RGB  = 0x0904, 		// '', color of triangle
		MODEL_CAM_MINPOS = 0x9005,  // STRUCT; Vec3; minimum position of the camera 
		MODEL_CAM_MAXPOS = 0x9006,  // ''; maximum position of the camera 
		MODEL_CAM_FOCUS  = 0x9007,  // ''; lookat point of camera
		MODEL_CAM_POS_OVR = 0x9010, // ''; overriden position of camera       |
		MODEL_CAM_TGT_OVR = 0x9011, // ''; overriden lookat point of camera   |> if these three options are all zero use random camera
		MODEL_CAM_UP_OVR = 0x9012,  // ''; overriden up vector of camera      |
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
		uint8_t status_size; // deprecated
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

#pragma pack (pop)

#define SLOT_CHECK_DEF(sn)	static_assert(sizeof(sn) <= 16, #sn " too large")

	SLOT_CHECK_DEF(TTCTime);
	SLOT_CHECK_DEF(TTCInfo);
	SLOT_CHECK_DEF(WeatherInfo);
	SLOT_CHECK_DEF(VStr);
	SLOT_CHECK_DEF(CalfixInfo);
	SLOT_CHECK_DEF(ClassInfo);
	SLOT_CHECK_DEF(PeriodInfo);

	enum struct UpdateStatusCode : uint8_t {
		NO_UPDATE = 0x00,
		DOWNLOADING_UPDATE = 0x01,  // downloading update, check percentage in other slot
		PREPARING_UPDATE = 0x02,	// preparing to update, writing files to the SD card, verifying CRCs
		ABOUT_TO_UPDATE = 0x03, 	// going to reset the STM soon, show a final warning
		// error flags, cleared after a bit
		UPDATE_DOWNLOAD_FAILED = 0x10, // download interrupted
		UPDATE_PREPARE_FAILED = 0x11, // update preparation failed
	};
}

#endif
