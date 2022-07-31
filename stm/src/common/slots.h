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
		WIFI_STATUS = 0x01,			// STRUCT; WifiStatus, info about wifi connection
		WEBUI_STATUS = 0x02,        // STRUCT; WebuiStatus, flag bitmask

		VIRTUAL_BUTTONMAP = 0x10, 	// UINT16_T; bitmap, override of the data on GPIOA for the buttons

		TTC_INFO = 0x20,			// STRUCT; TTCInfo
		TTC_NAME_1 = 0x21,			// STRING; name of bus/tram in slot 1; polled
		TTC_NAME_2 = 0x22,			// ''
		TTC_NAME_3 = 0x23,			// ''
		TTC_NAME_4 = 0x24,			// ''
		TTC_NAME_5 = 0x25,			// ''
		TTC_TIME_1a = 0x30,			// UINT64_T[]; TTCTime - time of next up to six buses (row a)
		TTC_TIME_2a = 0x31,			// ''
		TTC_TIME_3a = 0x32,			// ''
		TTC_TIME_4a = 0x33,			// ''
		TTC_TIME_5a = 0x34,			// ''
		TTC_TIME_1b = 0x3a,			// UINT64_T[]; TTCTime - time of next up to six buses (row b)
		TTC_TIME_2b = 0x3b,			// ''
		TTC_TIME_3b = 0x3c,			// ''
		TTC_TIME_4b = 0x3d,			// ''
		TTC_TIME_5b = 0x3e,			// ''
		TTC_ALERTSTR = 0x2a,        // STRING; current alerts

		WEATHER_ICON = 0x40,		// STRING; icon name from darksky
		WEATHER_INFO = 0x44,		// STRUCT; WeatherInfo
		WEATHER_STATUS = 0x45,		// STRING; weather status string
		WEATHER_ARRAY = 0x46,       // ENUM[]; WeatherStateArrayCode; list of ENUMS for the state per-hour
		WEATHER_TIME_SUN = 0x42,    // STRUCT; WeatherTimes - time for sunrise/sunset, used to show the info for hourlybar

		WEATHER_TEMP_GRAPH = 0x4a,  // INT16_T[]; feels like temp data per hour (/100)
		WEATHER_RTEMP_GRAPH = 0x4b, // INT16_T[]; real temp data per hour (/100)
		WEATHER_WIND_GRAPH = 0x4c,  // INT16_T[]; wind strength per hour (/100)

		WEATHER_HPREC_GRAPH = 0x4d, // STRUCT[]; PrecipData, temp data per hour 
		WEATHER_MPREC_GRAPH = 0x4e, // STRUCT[]; PrecipData, temp data per minute (decimated to every 2 minutes)

		MODEL_INFO = 0x900, 		// STRUCT; ModelInfo; number of triangles in the model
		MODEL_DATA = 0x901,         // STRUCT[]; Tri; entire model data
		MODEL_CAM_MINPOS  = 0x905,  // STRUCT; Vec3; minimum position of the camera 
		MODEL_CAM_MAXPOS  = 0x906,  // ''; maximum position of the camera 
		MODEL_CAM_FOCUS   = 0x907,  // STRUCT[]; Vec3; lookat points of camera
		
		PARCEL_NAMES = 0x50,        // STRING[]; name table for parcels
		PARCEL_INFOS = 0x51,        // STRUCT[]; ParcelInfo; parcel status information (0-indexed)
		PARCEL_STATUS_SHORT = 0x52, // STRING[]; status/location table for short view
		PARCEL_STATUS_LONG = 0x53,  // ''; status/location table for full view
		PARCEL_EXTRA_INFOS = 0x54,  // STRUCT[]; ExtraParcelInfoEntry; full package tracking logs (with some cutoff)

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

			INSTALLING_WEBUI_PACK = 32,

			LAST_RX_FAILED = 16,
		};
	};

	struct WifiStatus {
		bool connected;
		uint8_t ipaddr[4];
		uint8_t gateway[4];
	};

	struct TTCInfo {
		uint32_t flags;
		char altdircodes_a[5]; // direction code for times_a
		char altdircodes_b[5]; // direction code for times_b (if null, no data should be present there)
		uint8_t stopdistance[5]; // minutes to walk to a given stop. if 0, assume a sane default.

		enum Flags : uint32_t {
			EXIST_0 = (1 << 0),
			EXIST_1 = (1 << 1),  // the slot has data r.n.
			EXIST_2 = (1 << 2),
			EXIST_3 = (1 << 3),
			EXIST_4 = (1 << 4),

			SUBWAY_ALERT = (1 << 20), // there is an alert for the subway
			SUBWAY_DELAYED = (1 << 21), // we think the alert means there's a delay
			SUBWAY_OFF = (1 << 22), // we think the alert means the subway is currently broken / out of service / off
		};

		const static inline uint32_t EXIST_MASK = 0b11111;
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

	struct PrecipData {
		uint8_t is_snow; // is this preciptation snowy
		uint8_t probability; // from 0-255 as 0.0-1.0
		int16_t stddev; // precipitation error (0 if unknown) * 100
		int16_t amount; // mm / hr * 100
	};

	struct Vec3 {
		int16_t x;
		int16_t y;
		int16_t z;

		//!cfg: receives .x
		void set_x(float value) {
			x = (int16_t)(value * 512.f);
		}
		//!cfg: receives .y
		void set_y(float value) {
			y = (int16_t)(value * 512.f);
		}
		//!cfg: receives .z
		void set_z(float value) {
			z = (int16_t)(value * 512.f);
		}
	};

	struct Tri { // effective declaration for ESP only; actual triangle struct is used and declared in threed.h
		Vec3 p1, p2, p3;
		uint8_t r, g, b;
		uint8_t pad; // perhaps something relating to optimizations or some bs at some point, but this needs to align properly
	};

	struct ParcelStatusLine {
		uint16_t status_offset;  // offset into PARCEL_STATUS_SHORT
		uint16_t location_offset; // offset into PARCEL_STATUS_SHORT

		enum Flags : uint8_t {
			HAS_STATUS = 1,
			HAS_LOCATION = 2,
			HAS_EST_DEILIVERY = 4, // cannot be set on ExtraParcelInfoEntry
			HAS_UPDATED_TIME = 8,   
			EXTRA_INFO_TRUNCATED = 16, // cannot  be set on ExtraParcelInfoEntry
			EXTRA_INFO_MISSING   = 32 // cannot  be set on ExtraParcelInfoEntry
		};

		uint8_t flags;
	};

	struct ParcelInfo {
		uint16_t name_offset;    // offset into PARCEL_NAMES
		ParcelStatusLine status; // refd to PARCEL_STATUS_SHORT
	
		enum StatusIcon : uint8_t {
			UNK = 0,

			PRE_TRANSIT = 0x10,
			IN_TRANSIT  = 0x11,
			OUT_FOR_DELIVERY = 0x12,

			DELIVERED = 0x20,
			FAILED_TO_DELIVER = 0x21,
			READY_FOR_PICKUP = 0x22,

			RETURN_TO_SENDER = 0x30,
			GENERAL_ERROR = 0x31,
			CANCELLED = 0x32
		} status_icon;

		uint64_t updated_time;   // when was the last status received
		uint64_t estimated_delivery; // when do we expect to get the package
	};

	struct ExtraParcelInfoEntry {
		ParcelStatusLine status; // refd into PARCEL_STATUS_LONG
		uint8_t for_parcel; // which parcel is this an entry for
		uint64_t updated_time;
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
		HEAVY_SNOW,

		FOG = 0x40
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

			CONSOLE_MSG = 0x70,

			REFRESH_GRABBER = 0x80,
			SLEEP_ENABLE = 0x81
		};

		enum struct TimeStatus : uint8_t {
			Ok = 0,
			NotSet = 1,

			Timeout = 0xff
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

		enum struct GrabberID : uint8_t {
			TRANSIT = 0,
			WEATHER = 1,
			MODELSERVE = 2,
			CFGPULL = 3,
			PARCELS = 4,
			
			ALL = 0xfa,
			NONE = 0xfb
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
