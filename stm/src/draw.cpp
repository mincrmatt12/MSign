#include "draw.h"

namespace draw {

	uint16_t text_size(const char * text, const void * const font[], bool kern_on) {
		return text_size(reinterpret_cast<const uint8_t *>(text), font, kern_on);
	}

	void bitmap(matrix_type::framebuffer_type &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool mirror) {
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

	uint16_t text(matrix_type::framebuffer_type &fb, const uint8_t *text, const void * const font[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool kern_on) {
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

	void rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t r, uint8_t g, uint8_t b) {
		for (uint16_t x = x0; x < x1; ++x) {
			for (uint16_t y = y0; y < y1; ++y) {
				fb.r(x, y) = r;
				fb.g(x, y) = g;
				fb.b(x, y) = b;
			}
		}
	}

	void hatched_rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1) {
		for (uint16_t x = x0; x < x1; ++x) {
			for (uint16_t y = y0; y < y1; y += 2) {
				if (x % 2 == 0) {
					fb.r(x, y) = r0;
					fb.g(x, y) = g0;
					fb.b(x, y) = b0;
					if (y+1 < y1) {
						fb.r(x, y+1) = r1;
						fb.g(x, y+1) = g1;
						fb.b(x, y+1) = b1;
					}
				}
				else {
					fb.r(x, y) = r1;
					fb.g(x, y) = g1;
					fb.b(x, y) = b1;
					if (y+1 < y1) {
						fb.r(x, y+1) = r0;
						fb.g(x, y+1) = g0;
						fb.b(x, y+1) = b0;
					}
				}
			}
		}
	}

	uint16_t text(matrix_type::framebuffer_type &fb, const char * text, const void * const font[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool kern_on) {
		return ::draw::text(fb, reinterpret_cast<const uint8_t *>(text), font, x, y, r, g, b, kern_on);
	}

	int16_t search_kern_table(uint8_t a, uint8_t b, const int16_t * kern, const uint32_t size) {
		uint32_t start = 0, end = size;
		uint16_t needle = (((uint16_t)a << 8) + (uint16_t)b);
		while (start != end) {
			uint32_t head = (start + end) / 2;
			uint16_t current = ((uint16_t)(kern[head*3]) << 8) + (uint16_t)(kern[head*3 + 1]);
			if (current == needle) {
				return kern[head*3 + 2];
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

	uint16_t text_size(const uint8_t *text, const void * const font[], bool kern_on) {
		uint16_t len = 0;
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
				len += 2;
				continue;
			}
			if (c_prev != 0 && kern_table_size != 0 && kern_on) {
				len += search_kern_table(c_prev, c, kern, kern_table_size);
			}
			c_prev = c;
			if (data[c] == nullptr) continue; // invalid character
			len += *(metrics + (c * 6) + 3);
		}

		c = *(text - 1);
		len -= *(metrics + (c * 6) + 3);
		len += *(metrics + (c * 6));

		return len;
	}

	namespace detail {
		
		void line_impl_low(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b) {
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

		
		void line_impl_high(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b) {
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

	void fill(matrix_type::framebuffer_type &fb, uint8_t r, uint8_t g, uint8_t b) {
		rect(fb, 0, 0, matrix_type::framebuffer_type::width, matrix_type::framebuffer_type::height, r, g, b);
	}

	void line(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b) {
		if (std::abs(y1 - y0) < std::abs(x1 - x0)) {
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
