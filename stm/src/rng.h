#ifndef RNG_H
#define RNG_H

#include <stdint.h>

namespace rng {
	void init();
	uint32_t get();
	uint16_t getclr();

	inline static float getrange(float min, float max) {
		uint32_t prec = (max - min) * 1000;
		return (float(rng::get() % prec) / 1000.0) + min;
	}
}

#endif
