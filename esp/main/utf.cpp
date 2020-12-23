#include "utf.h"

#include <stdint.h>

namespace utf8 {

	// TODO: make this use a binary search.
	const uint32_t lookup[][2] = {
		{ 0x2013 /* – (EN DASH) */, (uint32_t)'-' },
		{ 0x2014 /* — (EM DASH) */, (uint32_t)'-' },
		{0, 0}
	};

	bool _lookup(uint8_t &out, uint32_t in) {
		for (int i = 0; i < (sizeof(lookup) / sizeof(lookup[0])); ++i) {
			if (lookup[i][0] == in) {
				out = (uint8_t)lookup[i][1];
				return true;
			}
		}

		return false;
	}

	void process(char * str) {
		uint32_t cur = 0;
		int remain = 0;
		char *out = str, *in = str;
		uint8_t byte;
		while ((byte = *in++) != 0) {
			// Remain = 0 when in normal state; remain > 1 for continutation; remain == 1 for last cont
			if (remain) {
				// if not a cont byte?
				if (byte >> 6 != 0b10) {
					// ignore
					remain = 0;
					cur = 0;
					*out++ = '?';
				}
				// otherwise, add to cur
				else {
					cur <<= 6;
					cur |= (byte & 077);
					// is this the last segment?
					if (!(--remain)) {
						// Add
						if (_lookup(byte, cur)) *out++ = byte;
						else *out++ = '?';
					}
				}
			}
			else {
				// Is this a start-of-multibyte marker?
				if (byte & (1 << 7)) {
					// Shift away continuation markers
					while (byte & (1 << 7)) {
						++remain;
						byte <<= 1;
					}
					// First continuation byte is this byte
					byte >>= remain--;
					// We've masked out all the upper bits by bitshifting, so just or.
					cur |= byte;
				}
				// Otherwise, just copy
				else {
					*out++ = byte;
				}
			}
		}
		// Null terminate the new string
		*out = 0;
	}
}
