#include "config.h"

namespace wifi {
	//!cfg: holds .time.zone, default "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00"
	extern config::string_t time_zone_str;

	//!cfg: holds .time.server, default "pool.ntp.org"
	extern config::string_t time_server;

	//!cfg: holds .wifi.force_memory_check, default true
	extern bool force_memory_check;

	// Wifi configuration data
	struct WifiSdConfig {
		//!cfg: holds .ssid
		config::string_t ssid;

		//!cfg: holds  .psk
		config::string_t psk;

		struct EnterpriseCfg {
			//!cfg: holds .username
			config::string_t _username;

			//!cfg: holds .identity
			config::string_t _identity;

			//!cfg: holds .password
			config::string_t _password;

			const char * username() {
				return _username.get();
			}

			const char * password() {
				return _password.get();
			}

			const char * identity() {
				if (!_identity) return username();
				return _identity.get();
			}

			operator bool() const {
				return _username && _password;
			}
		//!cfg: holds .enterprise
		} enterprise;

		//!cfg: holds .channel
		int channel = 0; // unknown

		//!cfg: holds .bssid
		uint8_t bssid[6] {};

		bool bssid_set;

		//!cfg: receives .bssid!size
		inline void _update_bssid_set(int value) {
			if (value == 6) bssid_set = true;
		}

		struct CountryCfg {
			CountryCfg() {
				_p0 = _p1 = _p2 = 0;
			}
			//!cfg: holds .schan
			uint8_t schan;

			//!cfg: holds .nchan
			uint8_t nchan;

			//!cfg: holds .max_power_db
			uint8_t max_power_db;

			//!cfg: holds .schan!present
			uint8_t _p0 : 1;
			//!cfg: holds .nchan!present
			uint8_t _p1 : 1;
			//!cfg: holds .max_power_db!present
			uint8_t _p2 : 1;

			char code[3] {};

			//!cfg: receives .code
			inline void _set_code(const char * value) {
				strncpy(code, value, 3);
			}

			operator bool() const {
				return _p0 && _p1 && _p2 && strlen(code) == 2;
			}
		//!cfg: holds .country
		} country;

		//!cfg: holds .hostname
		config::string_t hostname = "msign";
	};

	//!cfg: loads .wifi.conn
	bool load_wifi_config(WifiSdConfig& value);
}
