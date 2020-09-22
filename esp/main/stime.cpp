#include "stime.h"
#include "serial.h"
#include "config.h"
#include "common/slots.h"
#include "util.h"

void signtime::init() {
}

void signtime::start() {
/*	const char * time_server = config::manager.get_value(config::TIME_ZONE_SERVER, "pool.ntp.org");
	NTP.onNTPSyncEvent([](NTPSyncEvent_t e){active=true;});
	NTP.begin(time_server);
	Log.print("Starting timeserver with ");
	Log.println(time_server); */
}

void signtime::stop() {
	/* NTP.stop();
	active = false; */
}

/*
uint64_t signtime::get_time() {
	return 0
}

uint64_t signtime::to_local(uint64_t utc){
	return (torontoTZ.toLocal(utc));
}
uint64_t signtime::millis_to_local(uint64_t js_utc){
	return (js_utc % 1000) + to_local(js_utc / 1000) * 1000;
}
*/
