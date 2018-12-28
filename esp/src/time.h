#ifndef TIME_H
#define TIME_H

#include <stdint.h>

namespace time {
	void init();
	void start();
	void stop();
	uint64_t get_time();
	uint64_t to_local(uint64_t utc);
	uint64_t millis_to_local(uint64_t js_utc);
}

#endif
