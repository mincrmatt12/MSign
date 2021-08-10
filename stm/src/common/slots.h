#ifndef SLOTS_H
#define SLOTS_H

#include <stdint.h>
#include <string.h>

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
		WEBUI_STATUS = 0x02,        // STRUCT; WebuiStatus, flag bitmask

		VIRTUAL_BUTTONMAP = 0x10, 	// UINT16_T; bitmap, override of the data on GPIOA for the buttons

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
		WEATHER_TEMP_GRAPH = 0x41,  // INT16_T[]; temp data per hour (/100)
		WEATHER_TIME_SUN = 0x42,    // STRUCT; WeatherTimes - time for sunrise/sunset, used to show the info for hourlybar

		MODEL_INFO = 0x900, 		// STRUCT; ModelInfo; number of triangles in the model
		MODEL_DATA = 0x901,         // STRUCT[]; Tri; entire model data
		MODEL_CAM_MINPOS  = 0x905,  // STRUCT; Vec3; minimum position of the camera 
		MODEL_CAM_MAXPOS  = 0x906,  // ''; maximum position of the camera 
		MODEL_CAM_FOCUS   = 0x907,  // STRUCT[]; Vec3; lookat points of camera

		SCCFG_INFO = 0xb0, 			// STRUCT; ScCfgInfo, enabled screen bitmask, screen on/off
		SCCFG_TIMING = 0xb1,     	// STRUCT[]; ScCfgTime, how long to enable a certain screen 
	};

#pragma pack (push, 1)
	struct WebuiStatus {
		uint16_t flags;

		enum Flags : uint16_t {
			RECEIVING_SYSUPDATE = 1,
			RECEIVING_WEBUI_PACK = 2,
			RECEIVING_CERT_PACK = 4,

			LAST_RX_FAILED = 16,
		};
	};

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
		// all temperatures are stored in centidegrees celsius
		int16_t ctemp;
		int16_t ltemp;
		int16_t htemp;
		int16_t crtemp;
	};

	struct WeatherTimes {
		uint64_t sunrise, sunset;
	};

	struct VStr {
		uint8_t index;
		uint8_t size;
		uint8_t data[14];
	};

	struct ScCfgTime {
		uint32_t millis_enabled;
		enum ScreenId : uint8_t {
			TTC = 0,
			WEATHER = 1,
			MODEL = 2,
			PARCELS = 3,
			JENKINS = 4,
			PRINTER = 5
		} screen_id;
	};

	struct ScCfgInfo {
		uint16_t enabled_mask;
		bool display_on;

		enum EnabledMask : uint16_t {
			TTC = 1 << ScCfgTime::TTC,
			WEATHER = 1 << ScCfgTime::WEATHER,
			MODEL = 1 << ScCfgTime::MODEL,
			PARCELS = 1 << ScCfgTime::PARCELS,
			JENKINS = 1 << ScCfgTime::JENKINS,
			PRINTER = 1 << ScCfgTime::PRINTER
		};
	};

	struct ModelInfo {
		uint16_t tri_count;
		bool use_lighting;
	};

	struct Vec3 {
		//!cfg: holds .x, default 0.0
		float x;
		//!cfg: holds .y, default 0.0
		float y;
		//!cfg: holds .z, default 0.0
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
			DATA_FULFILL,
			DATA_RETRIEVE,
			DATA_REQUEST,
			DATA_SET_SIZE,
			DATA_STORE,

			ACK_DATA_TEMP = 0x30,
			ACK_DATA_FULFILL,
			ACK_DATA_RETRIEVE,
			ACK_DATA_REQUEST,
			ACK_DATA_SET_SIZE,
			ACK_DATA_STORE,

			QUERY_TIME = 0x40,

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

		enum struct DataStoreFulfillResult : uint8_t {
			Ok = 0,
			NotEnoughSpace_TryAgain,
			NotEnoughSpace_Failed,
			IllegalState = 0x10,
			InvalidOrNak = 0x11,

			Timeout = 0xff
		};

		enum struct UpdateStatus : uint8_t {
			ENTERED_UPDATE = 0x10,
			READY_FOR_IMAGE = 0x12,
			READY_FOR_CHUNK = 0x13,

			BEGINNING_COPY = 0x20,
			COPY_COMPLETED,

			RESEND_LAST_CHUNK_CSUM = 0x30,

			ABORT_CSUM = 0x40,
			PROTOCOL_ERROR
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

	// PACKET WRAPPER
	template<uint8_t CS=0> struct PacketWrapper;

	template<>
	struct PacketWrapper<0> {
		uint8_t direction;
		uint8_t size;
		uint8_t cmd_byte;

		const static inline uint8_t FromStm = 0xa5;
		const static inline uint8_t FromEsp = 0xa6;

		// Lots of helpers
		inline bool from_stm() const {return direction == FromStm;}
		inline bool from_esp() const {return direction == FromEsp;}
		inline operator bool() const {return direction >= 0xa5;}
		inline protocol::Command cmd() const {return static_cast<protocol::Command>(cmd_byte);}
		inline uint8_t * data() {return reinterpret_cast<uint8_t *>(this) + 3;}
		inline const uint8_t * data() const {return reinterpret_cast<const uint8_t *>(this) + 3;}

		// These could just be R& things, but i'm trying to avoid breaking aliasing.
		
		// Read helpers
		template<typename R>
		R at(uint8_t byte_offset) const {
			// for now, we use memcpy since it's more correct re: aliasing rules
			R res;
			memcpy(&res, data() + byte_offset, sizeof(R));
			return res;
		}

		// Write helper
		template<typename T>
		void put(T value, uint8_t byte_offset) {
			memcpy(data() + byte_offset, &value, sizeof(T));
		}

		PacketWrapper& operator=(const PacketWrapper& other) = delete;
		PacketWrapper& operator=(PacketWrapper&& other) = delete;
		PacketWrapper(const PacketWrapper& other) = delete;
		PacketWrapper(PacketWrapper&& other) = delete;
		PacketWrapper() = default;
	};

	// NOTE: this (the layout of inherited members) is fairly unstandardized, _however_ in c++23 this will be enforced.
	template<uint8_t ConstantSize>
	struct PacketWrapper : public PacketWrapper<0> {
		uint8_t _data[ConstantSize];

		inline void init(uint8_t cmd) {
#if (defined(STM32F205xx) || defined(STM32F207xx))
			direction = FromStm;
#else
			direction = FromEsp;
#endif
			cmd_byte = cmd;
			size = ConstantSize;
		}
	};

}

#endif
