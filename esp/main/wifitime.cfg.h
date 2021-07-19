
namespace wifi {
	//!cfg: receives .wifi.conn.$field
	void receive_config(const char *field, const char *value);

	//!cfg: holds .time.zone, default "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00"
	extern const char * time_zone_str;

	//!cfg: holds .time.server, default "pool.ntp.org"
	extern const char * time_server;
}
