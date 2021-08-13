#ifndef MSN_FXDMATH_H
#define MSN_FXDMATH_H

#include <stdint.h>
#include <stddef.h>
#include "../../intmath.h"

// m for math
namespace threed::m {
	struct fixed_t {
		const static size_t Fac = 15;
		constexpr static int32_t Mul = (1 << Fac);

		int32_t value;

		constexpr fixed_t(int non_fixed) : value(non_fixed * Mul) {}
		constexpr fixed_t(int64_t scaled_value, int factor) : value((scaled_value * Mul) / factor) {}
		constexpr fixed_t(int fixed_val, std::nullptr_t) : value(fixed_val) {}
		// fixed_t(float f) : value(f * (1 << Fac)) {}
		constexpr fixed_t() : value{} {};

		int32_t round() const {
			return intmath::round10(value, Mul);
		}

		int32_t ceil() const {
			return intmath::ceil10(value, Mul);
		}

		int32_t floor() const {
			return intmath::floor10(value, Mul);
		}

		int32_t trunc() const {
			return value / Mul;
		}

		explicit operator int32_t() const {
			return trunc();
		}

#define make_op(x) inline fixed_t operator x(int other) const { return fixed_t(value x other * Mul, nullptr); } \
			inline fixed_t& operator x##= (int other) {value x##= other * Mul; return *this;}

		make_op(+);
		make_op(-);

#undef make_op

#define make_op(x) inline fixed_t operator x(int other) const { return fixed_t(value x other, nullptr); } \
			inline fixed_t& operator x##= (int other) {value x##= other; return *this;}

		make_op(/);
		make_op(*);

#undef make_op

#define make_op(x) inline fixed_t operator x(fixed_t other) const { return fixed_t(value x other.value, nullptr); } \
			inline fixed_t& operator x##= (fixed_t other) {value x##= other.value; return *this;}

		make_op(+)
		make_op(-)

#undef make_op

		fixed_t operator*(fixed_t other) const {
			int64_t a = value;
			int64_t b = other.value;
			a *= b; // TODO: saturate this
			return fixed_t(a / Mul, nullptr);
		}

		fixed_t operator/(fixed_t other) const {
			int64_t a = value;
			int64_t b = other.value;
			a *= Mul; // TODO: saturate this
			return fixed_t(a / b, nullptr);
		}

#define make_op(x) inline fixed_t& operator x##= (fixed_t other) {return (*this = *this x other);}

		make_op(*)
		make_op(/)

#undef make_op

		fixed_t operator-() const {
			return fixed_t{-value, nullptr};
		}

#define make_op(x) inline bool operator x (fixed_t other) const {return value x other.value;} \
				   inline bool operator x (int     other) const {return value x (other * Mul);}

		make_op(<)
		make_op(>)
		make_op(<=)
		make_op(>=)
		make_op(==)
		make_op(!=)

#undef make_op
	};

	constexpr inline fixed_t PI = fixed_t(31415926, 10e6);
	constexpr inline fixed_t TWOPI = fixed_t(62831853, 10e6);
	constexpr inline fixed_t HALFPI = fixed_t(15707963, 10e6);

	fixed_t sin(fixed_t v);
	fixed_t cos(fixed_t v);
	fixed_t tan(fixed_t v);
	fixed_t sqrt(fixed_t v);

	fixed_t random(fixed_t min, fixed_t max);
}

#define make_op(x) inline threed::m::fixed_t operator x (int a, threed::m::fixed_t b) { return b x a; }

	make_op(+)
	make_op(*)

#undef make_op

#define make_op(x) inline auto operator x (int a, threed::m::fixed_t b) { return threed::m::fixed_t(a) x b; }

	make_op(-)
	make_op(/)

	make_op(<)
	make_op(>)
	make_op(<=)
	make_op(>=)
	make_op(==)
	make_op(!=)

#undef make_op

#endif
