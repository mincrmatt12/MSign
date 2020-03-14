#ifndef DRAW_H
#define DRAW_H

#include "matrix.h"
#include <cstdlib>

#ifndef SIM
typedef led::Matrix<led::FrameBuffer<128, 64, led::FourPanelSnake>> matrix_type;
#else
typedef led::Matrix<led::FrameBuffer<128, 64>> matrix_type;
#endif

namespace draw {
	// Gamma correction table
	extern const uint16_t gamma_cvt[256];

	// Perform gamma correction and 16-bit conversion on 8-bit color
	inline constexpr uint16_t cvt(uint8_t in) {
		return gamma_cvt[in];
	}
	
	// Draw a bitmap image to the framebuffer. Mirroring is based on bit ordering.
	void bitmap(matrix_type::framebuffer_type &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b, bool mirror=false); 

	int16_t search_kern_table(uint8_t a, uint8_t b, const int16_t * kern, const uint32_t size);

	// How large horizontally is the given text in the font.
	uint16_t text_size(const uint8_t *text, const void * const font[], bool kern_on=true); 
	uint16_t text_size(const char* text,    const void * const font[], bool kern_on=true);

	// Draw text to the framebuffer, returning the position where the next character would go.
	uint16_t text(matrix_type::framebuffer_type &fb, const uint8_t *text, const void * const font[], uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b, bool kern_on=true); 
	uint16_t text(matrix_type::framebuffer_type &fb, const char * text, const void * const font[], uint16_t x, uint16_t y, uint16_t r, uint16_t g, uint16_t b, bool kern_on=true); 

	// Draw a filled rectangle using exclusive coordinates (occupies x0 to x1 but not including x1)
	void rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t r, uint16_t g, uint16_t b); 

	// Draw a hatched rectangle using exclusive coordinates. Hatching will create a checkerboard pattern between the two colors provided.
	void hatched_rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t r0, uint16_t g0, uint16_t b0, uint16_t r1, uint16_t g1, uint16_t b1);

	// Fill the entire screen.
	void fill(matrix_type::framebuffer_type &fb, uint16_t r, uint16_t g, uint16_t b); 

	namespace detail {
		void line_impl_low(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t r, uint16_t g, uint16_t b); 
		void line_impl_high(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t r, uint16_t g, uint16_t b); 
	}
	
	// Draw a line including both endpoints
	void line(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t r, uint16_t g, uint16_t b); 
}

inline constexpr uint16_t operator ""_c(unsigned long long in) {
	return draw::cvt(in);
}

#endif
