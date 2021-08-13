#include "fixed.h"
#include "../../draw.h"
#include "../../rng.h"

namespace threed::m {
	fixed_t sin(fixed_t v) {
		return fixed_t(draw::fastsin((v / PI).value, fixed_t::Mul, fixed_t::Mul), nullptr);
	}

	fixed_t cos(fixed_t v) {
		return fixed_t(draw::fastsin(fixed_t::Mul / 2 - (v / PI).value, fixed_t::Mul, fixed_t::Mul), nullptr);
	}

	fixed_t tan(fixed_t v) {
		return sin(v) / cos(v);
	}

	fixed_t sqrt(fixed_t v) {
		if (v.value < 0) return 0; 

		// magic adapted from https://github.com/chmike/fpsqrt/blob/master/fpsqrt.c
		uint32_t t, q, b, r;

		r = v.value << (16 - fixed_t::Fac); // msign uses 15-bit fractional representations
		b = 0x40000000;
		q = 0;
		while (b > 0x40) {
			t = q + b;
			if (r >= t) {
				r -= t;
				q = t + b; // equivalent to q += 2*b
			}
			r <<= 1;
			b >>= 1;
		}
		q >>= 24 - fixed_t::Fac;
		return fixed_t(q, nullptr);
	}

	fixed_t random(fixed_t min, fixed_t max) {
		if (max < min) return random(max, min);

		uint32_t space = (max.value - min.value);
		int32_t val = rng::get() % space;
		return fixed_t{min.value + val, nullptr};
	}
}
