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
	
	void bitmap(matrix_type::framebuffer_type &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool mirror=false); 

	int16_t search_kern_table(uint8_t a, uint8_t b, const int16_t * kern, const uint32_t size);

	uint16_t text_size(const uint8_t *text, const void * const font[], bool kern_on=true); 
	uint16_t text_size(const char* text,    const void * const font[], bool kern_on=true);

	// returns where the next character would have been
	
	uint16_t text(matrix_type::framebuffer_type &fb, const uint8_t *text, const void * const font[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool kern_on=true); 

	
	uint16_t text(matrix_type::framebuffer_type &fb, const char * text, const void * const font[], uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b, bool kern_on=true); 

	
	void rect(matrix_type::framebuffer_type &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t r, uint8_t g, uint8_t b); 

	
	void fill(matrix_type::framebuffer_type &fb, uint8_t r, uint8_t g, uint8_t b); 

	namespace detail {
		void line_impl_low(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b); 
		void line_impl_high(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b); 
	}

	
	void line(matrix_type::framebuffer_type &fb, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b); 

}

#endif
