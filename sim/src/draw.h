#ifndef DRAW_H
#define DRAW_H

#include "matrix.h"
#include <cstdlib>

typedef led::Matrix<led::FrameBuffer<64, 32>> matrix_type;

namespace draw {
	template<typename FB>
	void bitmap(FB &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool mirror=false) {
		for (uint16_t i = 0, y0 = y; i < height; ++i, ++y0) {
			for (uint16_t j0 = 0, x0 = x; j0 < width; ++j0, ++x0) {
				uint16_t j = mirror ? (width - j0) - 1 : j0;
				uint8_t byte = j / 8;
				uint8_t bit = 1 << (7 - (j % 8));
				if ((bitmap[(i * stride) + byte] & bit) != 0) {
					fb.r(x0, y0) = r;
					fb.g(x0, y0) = g;
					fb.b(x0, y0) = b;
				}
			}
		}
	}

	int16_t search_kern_table(uint8_t a, uint8_t b, const int16_t * kern, const uint32_t size);

	uint16_t text_size(const uint8_t *text, const void * const font[], bool kern_on=true); 
	uint16_t text_size(const char* text,    const void * const font[], bool kern_on=true);

	// returns where the next character would have been
	template<typename FB>
	uint16_t text(FB &fb, const uint8_t *text, const void * const font[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool kern_on=true) {
		uint16_t pen = x;
		uint8_t c, c_prev = 0;

		auto data = reinterpret_cast<const uint8_t * const * const >(font[1]);
		auto metrics = reinterpret_cast<const int16_t *>(font[0]);
		const int16_t * kern;
		uint32_t kern_table_size = 0;
		if (font[2] != nullptr) {
			kern_table_size = *reinterpret_cast<const uint32_t *>(font[2]);
			kern = reinterpret_cast<const int16_t *>(font[3]);
		}
		while ((c = *(text++)) != 0) {
			if (c == ' ') {
				pen += 2;
				continue;
			}
			if (c_prev != 0 && kern_table_size != 0 && kern_on) {
				pen += search_kern_table(c_prev, c, kern, kern_table_size);
			}
			c_prev = c;
			if (data[c] == nullptr) continue; // invalid character
			bitmap(fb, data[c], *(metrics + (c * 6) + 0), *(metrics + (c * 6) + 1), *(metrics + (c * 6) + 2), pen + *(metrics + (c * 6) + 4), y - *(metrics + (c * 6) + 5), r, g, b);
			pen += *(metrics + (c * 6) + 3);
		}
		return pen;
	}

	template<typename FB>
	uint16_t text(FB &fb, const char * text, const void * const font[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool kern_on=true) {
		return ::draw::text(fb, reinterpret_cast<const uint8_t *>(text), font, x, y, r, g, b, kern_on);
	}

	template<typename FB>
	void rect(FB &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t r, uint8_t g, uint8_t b) {
		for (uint16_t x = x0; x < x1; ++x) {
			for (uint16_t y = y0; y < y1; ++y) {
				fb.r(x, y) = r;
				fb.g(x, y) = g;
				fb.b(x, y) = b;
			}
		}
	}

	template<typename FB>
	void fill(FB &fb, uint8_t r, uint8_t g, uint8_t b) {
		rect(fb, 0, 0, FB::width, FB::height, r, g, b);
	}

	namespace detail {
		template<typename FB>
		void line_impl_low(FB &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b) {
			int dx = x1 - x0;
			int dy = y1 - y0;
			int yi = 1;
			if (dy < 0) {
				yi = -1;
				dy = -dy;
			}
			int D = 2*dy - dx;
			int16_t y = y0;

			for (int16_t x = x0; x <= x1; ++x) {
				fb.r((uint16_t)x, (uint16_t)y) = r;
				fb.g((uint16_t)x, (uint16_t)y) = g;
				fb.b((uint16_t)x, (uint16_t)y) = b;
				if (D > 0) {
					y += yi;
					D -= 2*dx;
				}
				D += 2*dy;
			}
		}

		template<typename FB>
		void line_impl_high(FB &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b) {
			int dx = x1 - x0;
			int dy = y1 - y0;
			int xi = 1;
			if (dx < 0) {
				xi = -1;
				dx = -dx;
			}
			int D = 2*dx - dy;
			int16_t x = x0;

			for (int16_t y = y0; y <= y1; ++y) {
				fb.r((uint16_t)x, (uint16_t)y) = r;
				fb.g((uint16_t)x, (uint16_t)y) = g;
				fb.b((uint16_t)x, (uint16_t)y) = b;
				if (D > 0) {
					x += xi;
					D -= 2*dy;
				}
				D += 2*dx;
			}
		}
	}

	template<typename FB>
	void line(FB &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b) {
		if (abs(y1 - y0) < abs(x1 - x0)) {
			if (x0 > x1)
				detail::line_impl_low(fb, x1, y1, x0, y0, r, g, b);
			else
				detail::line_impl_low(fb, x0, y0, x1, y1, r, g, b);
		}
		else {
			if (y0 > y1)
				detail::line_impl_high(fb, x1, y1, x0, y0, r, g, b);
			else
				detail::line_impl_high(fb, x0, y0, x1, y1, r, g, b);
		}
	}

}

#endif
