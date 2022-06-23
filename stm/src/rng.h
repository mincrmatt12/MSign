#ifndef RNG_H
#define RNG_H

#include <stdint.h>
#include <type_traits>

namespace rng {
	void init();
	uint32_t get();
	uint16_t getclr();

	struct randengine {
		using result_type = uint32_t;

		constexpr inline result_type min() const { return 0; }
		constexpr inline result_type max() const { return INT32_MAX; }
		
		result_type operator()() {
			return get();
		}
	};
}

#endif
