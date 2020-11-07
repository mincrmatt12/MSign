#ifndef TESTFNT_FONT
#error please define font
#endif

#include <genfont.h>
#include <iostream>
#include <string.h>

namespace genfont = font::TESTFNT_FONT;

template<uint16_t Width, uint16_t Height>
struct FrameBuffer {
	FrameBuffer() {
		memset(data, 0x0, sizeof(data));
	}

	void show() {                                                                                                                                                             
		std::cout << "\e[?25l";                                                                                                                                                 
		for (size_t y = 0; y < Height; ++y) {                                                                                                                                 
			for (size_t x = 0; x < Width; ++x) {                                                                                                                              
				std::cout << "\x1b[48;2;" << std::to_string(_remap(_r(x, y))) << ";" << std::to_string(_remap(_g(x, y))) << ";" << std::to_string(_remap(_b(x, y))) << "m  "; 
			}                                                                                                                                                                 
			std::cout << std::endl;                                                                                                                                           
		}                                                                                                                                                                     
		std::cout << "\e[?25h";
	}                                                                                                                                                                         

	uint8_t & r(uint16_t x, uint16_t y) {
		if (on_screen(x, y))
			return _r(x, y);
		return junk;
	}
	uint8_t & g(uint16_t x, uint16_t y) {
		if (on_screen(x, y))
			return _g(x, y);
		return junk;
	}
	uint8_t & b(uint16_t x, uint16_t y) {
		if (on_screen(x, y))
			return _b(x, y);
		return junk;
	}

	const uint8_t & r(uint16_t x, uint16_t y) const {
		if (on_screen(x, y))
			return _r(x, y);
		return junk;
	}
	const uint8_t & g(uint16_t x, uint16_t y) const {
		if (on_screen(x, y))
			return _g(x, y);
		return junk;
	}
	const uint8_t & b(uint16_t x, uint16_t y) const {
		if (on_screen(x, y))
			return _b(x, y);
		return junk;
	}

	inline bool on_screen(uint16_t x, uint16_t y) const {
		return (x < Width && y < Height);
	}

	static constexpr uint16_t width = Width;
	static constexpr uint16_t height = Height;

	private:
	uint8_t data[Width*Height*3];

	inline uint8_t & _r(uint16_t x, uint16_t y) {
		return data[x*3 + y*Width*3 + 0];
	}

	inline uint8_t & _g(uint16_t x, uint16_t y) {
		return data[x*3 + y*Width*3 + 1];
	}

	inline uint8_t & _b(uint16_t x, uint16_t y) {
		return data[x*3 + y*Width*3 + 2];
	}

	inline uint8_t _remap(uint8_t prev) {                                    
		return prev;
	}                                                                        


	uint8_t junk; // used as failsafe for read/write out of bounds
};

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
		fb.r(pen, y) = 255;
		if (c == ' ') {
			c_prev = ' ';
			pen += 2;
			continue;
		}
		if (c_prev != 0 && kern_table_size != 0 && kern_on) {
			auto offset = search_kern_table(c_prev, c, kern, kern_table_size);
			std::cout << c_prev << c << "of: " << offset << std::endl;
			pen += offset;
			fb.b(pen, y + 1) = 255;
		}
		// show yTop
		fb.g(pen, y - metrics[(c*6) + 5]) = 255;
		fb.b(pen, y - metrics[(c*6) + 5]) = 127;
		// show xTop
		fb.g(pen + metrics[(c*6) + 4], y+2) = 127;
		fb.r(pen + metrics[(c*6) + 4], y+2) = 255;
		c_prev = c;
		if (data[c] == nullptr) continue; // invalid character
		bitmap(fb, data[c], metrics[(c*6) + 0], metrics[(c*6) + 1], metrics[(c*6) + 2], pen + metrics[(c*6) + 4], y - metrics[(c*6) + 5], r, g, b);
		pen += metrics[(c*6) + 3];
	}
	return pen;
}

int main(int argc, char ** argv) {
	const char * to_draw = (argc == 2) ? argv[1] : "T!est /string\\ abc_123.";

	FrameBuffer<128, 64> fb;
	int result = text(fb, (const uint8_t *)to_draw, genfont::info, 0, 20, 255, 255, 255);
	std::cout << std::endl;
	std::cout << std::endl;

	fb.show();

	std::cout << std::endl;
	std::cout << std::endl;

	return result;
}
