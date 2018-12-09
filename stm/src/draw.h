#ifndef DRAW_H
#define DRAW_H

#include "matrix.h"

namespace draw {
	template<typename FB>
	void bitmap(FB &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
		for (uint16_t i = 0, y0 = y; i < height; ++i, ++y0) {
			for (uint16_t j = 0, x0 = x; j < width; ++j, ++x0) {
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

	// returns where the next character would have been
	template<typename FB>
	uint16_t text(FB &fb, const uint8_t *text, const void * font[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool kern_on=true) {
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

	int16_t search_kern_table(uint8_t a, uint8_t b, const int16_t * kern, const uint32_t size) {
		uint32_t start = 0, end = size;
		uint16_t needle = ((uint16_t)a + ((uint16_t)b << 8));
		while (start != end) {
			uint32_t head = (start + end) / 2;
			uint16_t current = (uint16_t)(*(kern + head*3)) + (((uint16_t)(*(kern + head*3 + 1))) << 8);
			if (current == needle) {
				return *(kern + head*3 + 2);
			}
			else {
				if (start - end == 1 || end - start == 1) {
					return 0;
				}
				if (current > needle) {
					end = head;
				}
				else {
					start = head;
				}
			}
		}
		return 0;
	}

	template<typename FB>
	uint16_t text(FB &fb, const char * text, const void *font[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool kern_on=true) {
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
}

#endif
