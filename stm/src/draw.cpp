#include "draw.h"

namespace draw {
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
}
