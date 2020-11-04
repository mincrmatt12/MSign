#include "mindraw.h"
#include "../draw.h"
#include "../fonts/lcdpixel_6.h"

namespace crash::draw {
	uint16_t text(Matrix &fb, const uint8_t *text, uint16_t x, uint16_t y, uint8_t color) {
		uint16_t pen = x;
		uint8_t c;

		const static void * const * const font = font::lcdpixel_6::info;

		auto data = reinterpret_cast<const uint8_t * const * const >(font[1]);
		auto metrics = reinterpret_cast<const int16_t *>(font[0]);
		while ((c = *(text++)) != 0) {
			if (c == ' ') {
				pen += 2;
				continue;
			}
			bitmap(fb, data[c], *(metrics + (c * 6) + 0), *(metrics + (c * 6) + 1), *(metrics + (c * 6) + 2), pen + *(metrics + (c * 6) + 4), y - *(metrics + (c * 6) + 5), color);
			pen += *(metrics + (c * 6) + 3);
		}
		return pen;
	}

	uint16_t text(Matrix &fb, const char *text, uint16_t x, uint16_t y, uint8_t color) {
		return ::crash::draw::text(fb, reinterpret_cast<const uint8_t *>(text), x, y, color);
	}

	size_t break_line(const char* text, uint16_t start) {
		uint16_t pen = start;
		uint8_t c;

		const static void * const * const font = font::lcdpixel_6::info;
		auto metrics = reinterpret_cast<const int16_t *>(font[0]);

		size_t amt = 0;
		while ((c = *(++amt, text++)) != 0 && pen < 122) {
			if (c == ' ') {
				pen += 2;
				continue;
			}
			pen += *(metrics + (c * 6) + 3);
		}
		return amt;
	}

	void bitmap(Matrix &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, uint8_t color) {
		for (uint16_t i = 0, y0 = y; i < height; ++i, ++y0) {
			for (uint16_t j0 = 0, x0 = x; j0 < width; ++j0, ++x0) {
				uint8_t byte = j0 / 8;
				uint8_t bit = 1 << (7 - (j0 % 8));
				if ((bitmap[(i * stride) + byte] & bit) != 0) {
					if (x0 >= 128 || y0 >= 64) continue;
					fb.color(x0, y0) = color;
				}
			}
		}
	}
	
	void rect(Matrix &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t color) {
		for (uint16_t x = x0; x < x1; ++x) {
			for (uint16_t y = y0; y < y1; ++y) {
				if (x >= 128 || y >= 64) continue;
				fb.color(x, y) = color;
			}
		}
	}
}
