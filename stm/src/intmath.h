#ifndef INTMATH_H
#define INTMATH_H

#include <stdint.h>
#include <type_traits>

namespace intmath {
	// Round to nearest 10^N and divide.
	//
	// round10(453, 100) --> 4.53 --> [5]
	//         |    |
	//         |    \-------\
	//         |            |
	//         \-- = 4.53 * 100
	template<typename T>
	T round10(T v, T pos=100) {
		if constexpr (std::is_signed_v<T>) {
			T res = v / pos, rem = v % pos;
			if (-(pos / 2) < rem && rem < (pos / 2)) return res;
			else if (rem >= (pos / 2)) return res + (T)1;
			else return res - (T)1;
		}
		else {
			if (v % pos >= pos / 2) return (v / pos) + (T)1;
			else return (v / pos);
		}
	}

	// Same as round10, except rounds to lowest. Multiply by pos again to keep fixedpoint.
	template<typename T>
	T floor10(T v, T pos=100) {
		if constexpr (std::is_signed_v<T>) {
			T res = v / pos, rem = v % pos;
			if (rem >= 0) return res;
			else return res - (T)1;
		}
		else {
			return (v / pos);
		}
	}

	// Same as round10, except rounds to lowest. Multiply by pos again to keep fixedpoint.
	template<typename T>
	T ceil10(T v, T pos=100) {
		if constexpr (std::is_signed_v<T>) {
			T res = v / pos, rem = v % pos;
			if (rem <= 0) return res;
			else return res + (T)1;
		}
		else {
			return ((v + (pos - 1)) / pos);
		}
	}
}

#endif