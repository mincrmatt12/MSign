#ifndef TIME_H
#define TIME_H

#include <stdint.h>

namespace time {
	void init();
	void start();
	void stop();
	uint64_t get_time();
}

#endif
