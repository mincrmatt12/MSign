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

		REMOTE_CONTROL = 0x10,      // BOOL; 0 - no remote control is taking place, request_screen and virtual_buttonmap are not valid
		REQUEST_SCREEN = 0x11, 		// UINT8_T; screen ID to request, sent very infrequently
		VIRTUAL_BUTTONMAP = 0x12, 	// UINT8_T; bitmap, override of the data on GPIOA for the buttons

		TTC_INFO = 0x20,			// STRUCT; TTCInfo
		TTC_NAME_1 = 0x21,			// STRING; name of bus/tram in slot 1; polled
		TTC_NAME_2 = 0x22,			// ''
		TTC_NAME_3 = 0x23,			// ''
		TTC_TIME_1 = 0x24,			// UINT64_T[]; TTCTime - time of next up to six buses
		TTC_TIME_2 = 0x25,			// ''
		TTC_TIME_3 = 0x26,			// ''
		TTC_ALERTSTR = 0x2a,        // STRING; current alerts

		WEATHER_ICON = 0x40,		// STRING; icon name from darksky
		WEATHER_INFO = 0x44,		// STRUCT; WeatherInfo
		WEATHER_STATUS = 0x45,		// STRING; weather status string
		WEATHER_ARRAY = 0x46,       // STRING; list of ENUMS for the state per-hour
		WEATHER_TEMP_GRAPH = 0x41,  // FLOAT[]; temp data per hour
		WEATHER_TIME_SUN = 0x42,    // STRUCT; WeatherTimes - time for sunrise/sunset, used to show the info for hourlybar

		CALFIX_INFO = 0x50,			// STRUCT; CalfixInfo, current school day and schedule as int
		CALFIX_CLS1 = 0x51,			// STRUCT; ClassInfo, name and room
		CALFIX_CLS2 = 0x52,			// ''
		CALFIX_CLS3 = 0x53,			// ''
		CALFIX_CLS4 = 0x54,			// ''
		CALFIX_PRDH1 = 0x55, 		// STRUCT; PeriodInfo, start time for period 1 & 2
		CALFIX_PRDH2 = 0x56, 		// '', 3 & 4

		MODEL_INFO = 0x900, 		// STRUCT; ModelInfo; number of triangles in the model
		MODEL_DATA = 0x901,         // STRUCT[]; Tri; entire model data
		MODEL_CAM_MINPOS  = 0x905,  // STRUCT; Vec3; minimum position of the camera 
		MODEL_CAM_MAXPOS  = 0x906,  // ''; maximum position of the camera 
		MODEL_CAM_FOCUS1  = 0x907,  // ''; lookat point of camera
		MODEL_CAM_FOCUS2  = 0x908,  // ''; '' 2
		MODEL_CAM_FOCUS3  = 0x909,  // ''; '' 3

		SCCFG_INFO = 0xb0, 			// STRUCT; ScCfgInfo, enabled screen bitmask, screen on/off
		SCCFG_TIMING = 0xb1,     	// STRUCT[]; ScCfgTime, how long to enable a certain screen 
	};

#pragma pack (push, 1)
	struct TTCInfo {
		uint16_t flags;

		enum Flags : uint16_t {
			ALERT_0 = 1,
			ALERT_1 = 2,  // there is an alert, and the relevant message is available in the VSTR.
			ALERT_2 = 4,

			EXIST_0 = 8,
			EXIST_1 = 16,  // the slot has data r.n.
			EXIST_2 = 32,

			DELAY_0 = 64,
			DELAY_1 = 128, // the slot is reported as delayed
			DELAY_2 = 256,

			SUBWAY_ALERT = 512, // there is an alert for the subway
			SUBWAY_DELAYED = 1024, // we think the alert means there's a delay
			SUBWAY_OFF = 2048, // we think the alert means the subway is currently broken / out of service / off
		};
	};

	struct WeatherInfo {
		float ctemp;
		float ltemp;
		float htemp;
		float crtemp;
	};

	struct WeatherTimes {
		uint64_t sunrise, sunset;
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
			PARCELS = 16,
			JENKINS = 32,
			PRINTER = 64
		};
	};

	struct ScCfgTime {
		uint32_t millis_enabled;
		enum ScreenId : uint8_t {
			TTC = 0,
			WEATHER = 1,
			MODEL = 2,
			PERIODS = 3,
			PARCELS = 4,
			JENKINS = 5,
			PRINTER = 6
		} screen_id;
	};

	struct ModelInfo {
		uint16_t tri_count;
		bool use_lighting;
	};

	struct Vec3 {
		float x;
		float y;
		float z;
	};

	struct Tri { // effective declaration for ESP only; actual triangle struct is used and declared in threed.h
		Vec3 p1, p2, p3;
		uint8_t r, g, b;
		uint8_t pad; // perhaps something relating to optimizations or some bs at some point, but this needs to align properly
	};

#pragma pack (pop)

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

	// PROTOCOL DEFINITIONS
	
	namespace protocol {
		enum Command : uint8_t {
			HANDSHAKE_INIT = 0x10,
			HANDSHAKE_RESP,
			HANDSHAKE_OK,
			HANDSHAKE_UOK,

			DATA_TEMP = 0x20,
			DATA_UPDATE,
			DATA_MOVE,
			DATA_DEL,
			DATA_FORGOT,

			ACK_DATA_TEMP = 0x30,
			ACK_DATA_UPDATE,
			ACK_DATA_MOVE,
			ACK_DATA_DEL,

			QUERY_FREE_HEAP = 0x40,
			QUERY_TIME,

			RESET = 0x50,
			PING,
			PONG,
			
			UPDATE_CMD = 0x60,
			UPDATE_IMG_DATA,
			UPDATE_IMG_START,
			UPDATE_STATUS,

			CONSOLE_MSG = 0x70
		};

		enum struct TimeStatus : uint8_t {
			Ok = 0,
			NotSet = 1
		};

		enum struct UpdateStatus : uint8_t {
			ENTERED_UPDATE = 0x10,
			READY_FOR_IMAGE = 0x12,
			READY_FOR_CHUNK = 0x13,

			BEGINNING_COPY = 0x20,
			COPY_COMPLETED,

			RESEND_LAST_CHUNK_CSUM = 0x30,

			ABORT_CSUM = 0x40
		};

		enum struct UpdateCmd : uint8_t {
			CANCEL_UPDATE = 0x10,
			PREPARE_FOR_IMAGE,

			ERR_IMG_READ = 0x30,
			ERR_IMG_CSUM,
			ERR_STATUS,

			UPDATE_COMPLETED_OK = 0x40,

			ESP_WROTE_SECTOR = 0x50,
			ESP_COPYING
		};
	}
}

#endif
