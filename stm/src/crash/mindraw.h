#ifndef MSN_MINDRAW_H
#define MSN_MINDRAW_H

// bare minimum draw functions
#include <stdint.h>
#include "simplematrix.h"

namespace crash::draw {
	// Draw a bitmap image to the framebuffer. Mirroring is based on bit ordering.
	void bitmap(Matrix &fb, const uint8_t * bitmap, uint8_t width, uint8_t height, uint8_t stride, uint16_t x, uint16_t y, uint8_t color); 

	// Draw text to the framebuffer, returning the position where the next character would go.
	uint16_t text(Matrix &fb, const uint8_t *text, uint16_t x, uint16_t y, uint8_t color); 
	uint16_t text(Matrix &fb, const char * text, uint16_t x, uint16_t y, uint8_t color); 

	// Draw a filled rectangle using exclusive coordinates (occupies x0 to x1 but not including x1)
	void rect(Matrix &fb, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t color); 
}

#endif
