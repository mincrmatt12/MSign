#ifndef TTC_H
#define TTC_H

#include "common/slots.h"
#include "json.h"

namespace ttc {
	void init();
	void loop();

	void do_update(const char * stop_id, uint8_t slot);
	void on_open(uint16_t data_id);

	slots::TTCInfo info;
	slots::TTCTime times[3];

	uint64_t time_since_last_update = 0;
}

#endif
