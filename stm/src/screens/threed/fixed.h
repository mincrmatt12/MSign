#ifndef MSN_FXDMATH_H
#define MSN_FXDMATH_H

#include <stdint.h>
#include <stddef.h>
#include "../../intmath.h"
#include <bit>

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

		constexpr int32_t round() const {
			return intmath::round10(value, Mul);
		}

		constexpr int32_t fast_unsigned_round() const {
			return (value + Mul / 2) / Mul;
		}

		constexpr int32_t ceil() const {
			return intmath::ceil10(value, Mul);
		}

		constexpr int32_t floor() const {
			return intmath::floor10(value, Mul);
		}

		constexpr int32_t trunc() const {
			return value / Mul;
		}

		constexpr explicit operator int32_t() const {
			return trunc();
		}

#define make_op(x) constexpr inline fixed_t operator x(int other) const { return fixed_t(value x other * Mul, nullptr); } \
			constexpr inline fixed_t& operator x##= (int other) {value x##= other * Mul; return *this;}

		make_op(+);
		make_op(-);

#undef make_op

#define make_op(x) constexpr inline fixed_t operator x(int other) const { return fixed_t(value x other, nullptr); } \
			constexpr inline fixed_t& operator x##= (int other) {value x##= other; return *this;}

		make_op(/);
		make_op(*);

#undef make_op

#define make_op(x) constexpr inline fixed_t operator x(fixed_t other) const { return fixed_t(value x other.value, nullptr); } \
			constexpr inline fixed_t& operator x##= (fixed_t other) {value x##= other.value; return *this;}

		make_op(+)
		make_op(-)

#undef make_op

		constexpr fixed_t operator*(fixed_t other) const {

			// Converted to assembly because gcc is dumb and doesn't know how to use shift rights properly..
			//
			// Implements:
#ifdef SIM
			int64_t res = (int64_t)value * (int64_t)other.value; 
			return fixed_t(res / Mul, nullptr);
#else
			if (std::is_constant_evaluated()) {
				int64_t res = (int64_t)value * (int64_t)other.value; 
				return fixed_t(res / Mul, nullptr);
			}
			
			uint32_t result_value;
			uint32_t _scratch;
			
			// implemented without naked to try and let gcc optimize out loads
			asm (
				"smull %[ResultValue], %[ClobberB], %[InputA], %[InputB]\n\t" // (ClobberB:ResultValue) [res] = (a*b)
				"bfi %[ResultValue], %[ClobberB], #0, %[Fac]\n\t" // copy low Fac bits of the high word into the low word
				: [ResultValue] "=r" (result_value), [ClobberB] "=r"(_scratch) // output
				: [InputA] "r" (value), [InputB] "r" (other.value), [Fac] "i" (Fac) // input
			);
			
			return fixed_t((int32_t)std::rotr(result_value, Fac), nullptr); // This rotate is performed in c to let gcc potentially optimize it into a following alu op2
#endif
		}

		constexpr fixed_t reciprocal() const {
			return fixed_t((1 << (2 * Fac)) / value, nullptr);
		}

		constexpr fixed_t operator/(fixed_t other) const {
#ifdef FIXED_USE_HIGHPREC
			int64_t a = (int64_t)value * (int64_t)Mul; // done here to try and get the compiler to use SMULL
			int64_t b = other.value;
			return fixed_t(a / b, nullptr);
#else
			return (*this) * other.reciprocal();
#endif
		}

#define make_op(x) inline constexpr fixed_t& operator x##= (fixed_t other) {return (*this = *this x other);}

		make_op(*)
		make_op(/)

#undef make_op

		constexpr fixed_t operator-() const {
			return fixed_t{-value, nullptr};
		}

#define make_op(x) constexpr inline bool operator x (fixed_t other) const {return value x other.value;} \
				   constexpr inline bool operator x (int     other) const {return value x (other * Mul);}

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

namespace screen {
	namespace fm = ::threed::m;
}

#endif
