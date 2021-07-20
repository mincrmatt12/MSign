#ifndef MSN_COLOR_H
#define MSN_COLOR_H

#include <stdint.h>
#include <utility>
#include <algorithm>

namespace led {
	struct color_t {
		constexpr color_t(uint16_t r, uint16_t g, uint16_t b) :
			r(r), g(g), b(b) {}

		constexpr color_t() : r(0), g(0), b(0) {}
		constexpr color_t(uint16_t gray) : r(gray), g(gray), b(gray) {}

		union {
			struct {
				uint16_t r;
				uint16_t g;
				uint16_t b;
			};
			uint16_t components[3];
		};

		uint16_t get_spare() {
			return ((r & 0xf000) >> 4) | ((g & 0xf000) >> 8) | ((b & 0xf000) >> 12);
		}

		void set_spare(uint16_t value) {
			r = (r & 0x0fff) | (value & 0x0f00) << 4;
			g = (g & 0x0fff) | (value & 0x00f0) << 8;
			b = (b & 0x0fff) | (value & 0x000f) << 12;
		}
	};
}

#endif
