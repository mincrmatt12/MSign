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

		constexpr inline static result_type min() { return 0; }
		constexpr inline static result_type max() { return INT32_MAX; }
		
		result_type operator()() {
			return get();
		}
	};
}

#endif
