#ifndef TTC_H
#define TTC_H

#include "common/slots.h"

namespace ttc {
	void init();
	void loop();

	bool do_update(const char * stop_id, const char * dtag, uint8_t slot);
	void do_alert_update();
	void on_open(uint16_t data_id);

	extern slots::TTCInfo info;
	extern slots::TTCTime times[6];
	extern char * alert_buffer;

	extern uint64_t time_since_last_update;
}

#endif
