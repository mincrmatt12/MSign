#ifndef DRAW_H
#define DRAW_H

#include "matrix.h"
#include <cmath>
#include <cstdlib>

#ifndef SIM
typedef led::Matrix<led::FrameBuffer<128, 64, led::FourPanelSnake>> matrix_type;
#else
typedef led::Matrix<led::FrameBuffer<128, 64>> matrix_type;
#endif

namespace draw {
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

	// Gamma correction table
	extern const uint16_t gamma_cvt[256];

	// Perform gamma correction and 16-bit conversion on 8-bit color
	inline uint16_t cvt(uint8_t in) {
		return gamma_cvt[in];
	}
	
	// Easing/scrolling helper functions.
	//
	// These generally take a timebase to use and content size params.
	
	// Scrolls content. Output moves backwards, so for left-right scrolling it goes from the right.
	inline int16_t scroll(int64_t timebase, int16_t content_size) {
		timebase %= (content_size + matrix_type::framebuffer_type::width) + 1;
		timebase =  ((content_size + matrix_type::framebuffer_type::width) + 1) - timebase;
		timebase -= content_size;
		return timebase;
	}

	// ```
	//           .......            --U
	//         ..| tS  |..
	//        .           .
	//       .             .
	//     ..               ..| tS |
	// .... | tT |            ......--0
	// ```
	//
	// Creates a wave of the above pattern. Generally used to show two "screens" of data at once.
	inline int16_t distorted_ease_wave(int64_t timebase, int64_t tT, int64_t tS, int16_t U) {
		static const int64_t period = (tT + tS), twoperiod = 2*(tT + tS);
		bool isU = (timebase % twoperiod) < period;

		if (timebase % period < tS) return isU * U;

		float t = ((timebase % period) - tS) / (float)tT;
		if (isU) 		
			return (1 - powf(t, 2.4) / (powf(t, 2.4) + powf(1 - t, 2.4))) * U;
		else
			return U * powf(t, 2.4) / (powf(t, 2.4) + powf(1 - t, 2.4));
	}
}

inline constexpr uint16_t operator ""_c(unsigned long long in) {
	constexpr uint16_t gamma_cvt[256] = {
#define GAMMA_TABLE 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, \
	3, 4, 5, 6, 7, 8, 9, 11, 12, 14, \
	15, 17, 19, 21, 23, 25, 27, 29, 32, 34, \
	37, 40, 43, 46, 49, 52, 55, 59, 62, 66, \
	70, 73, 77, 82, 86, 90, 95, 99, 104, 109, \
	114, 119, 124, 129, 135, 140, 146, 152, 158, 164, \
	170, 176, 182, 189, 196, 202, 209, 216, 224, 231, \
	238, 246, 254, 261, 269, 277, 286, 294, 302, 311, \
	320, 328, 337, 347, 356, 365, 375, 384, 394, 404, \
	414, 424, 435, 445, 456, 467, 477, 488, 500, 511, \
	522, 534, 545, 557, 569, 581, 594, 606, 619, 631, \
	644, 657, 670, 683, 697, 710, 724, 738, 752, 766, \
	780, 794, 809, 823, 838, 853, 868, 884, 899, 914, \
	930, 946, 962, 978, 994, 1011, 1027, 1044, 1061, 1078, \
	1095, 1112, 1130, 1147, 1165, 1183, 1201, 1219, 1237, 1256, \
	1274, 1293, 1312, 1331, 1350, 1370, 1389, 1409, 1429, 1449, \
	1469, 1489, 1509, 1530, 1551, 1572, 1593, 1614, 1635, 1657, \
	1678, 1700, 1722, 1744, 1766, 1789, 1811, 1834, 1857, 1880, \
	1903, 1926, 1950, 1974, 1997, 2021, 2045, 2070, 2094, 2119, \
	2143, 2168, 2193, 2219, 2244, 2270, 2295, 2321, 2347, 2373, \
	2400, 2426, 2453, 2479, 2506, 2534, 2561, 2588, 2616, 2644, \
	2671, 2700, 2728, 2756, 2785, 2813, 2842, 2871, 2900, 2930, \
	2959, 2989, 3019, 3049, 3079, 3109, 3140, 3170, 3201, 3232, \
	3263, 3295, 3326, 3358, 3390, 3421, 3454, 3486, 3518, 3551, \
	3584, 3617, 3650, 3683, 3716, 3750, 3784, 3818, 3852, 3886, \
	3920, 3955, 3990, 4025, 4060, 4095
GAMMA_TABLE
	};
	return gamma_cvt[in];
}

#endif
