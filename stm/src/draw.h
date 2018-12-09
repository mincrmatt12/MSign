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

	// returns where the next character would have been
	template<typename FB>
	uint16_t text(FB &fb, uint8_t *text, const int16_t metrics[255][6], const uint8_t * data[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
		uint16_t pen = x;
		uint8_t c;
		while ((c = *(text++)) != 0) {
			if (c == ' ') {
				pen += 2;
				continue;
			}
			if (data[c] == nullptr) continue; // invalid character
			bitmap(fb, data[c], metrics[c][0], metrics[c][1], metrics[c][2], pen + metrics[c][4], y - metrics[c][5], r, g, b);
			pen += metrics[c][3];
		}
		return pen;
	}

	template<typename FB>
	uint16_t text(FB &fb, char * text, const int16_t metrics[255][6], const uint8_t *data[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
		return ::draw::text(fb, reinterpret_cast<uint8_t *>(text), metrics, data, x, y, r, g, b);
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
