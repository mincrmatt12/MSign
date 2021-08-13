#ifndef RNG_H
#define RNG_H

#include <stdint.h>

namespace rng {
	void init();
	uint32_t get();
	uint16_t getclr();
}

#endif
