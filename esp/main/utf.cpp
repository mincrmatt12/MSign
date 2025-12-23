#include "utf.h"

#include <stdint.h>

namespace utf8 {
	struct lookup_entry {
		uint32_t uni;
		char asc;
		uint8_t range = 1;
	};

	const lookup_entry lookup[] = {
		{ .uni = 0x2013 /* EN DASH */, .asc='-' },
		{ .uni = 0x2014 /* EM DASH */, .asc='-' },
		{ .uni = 0x2018 /* LEFT SINGLE QUOTATION MARK */, .asc='\'' },
		{ .uni = 0x2019 /* RIGHT SINGLE QUOTATION MARK */, .asc='\'' },
		{ .uni = 0x201C /* LEFT DOUBLE QUOTATION MARK */, .asc='"' },
		{ .uni = 0x201D /* RIGHT DOUBLE QUOTATION MARK */, .asc='"' },
		{ .uni = 0x3010 /* LEFT BLACK LENTICULAR BRACKET */, .asc='[' },
		{ .uni = 0x3011 /* RIGHT BLACK LENTICULAR BRACKET */, .asc=']' },
		{ .uni = 0xFF01 /* fullwidth chars (FF01-FF60) */, .asc='!', .range = 0x60 },
	};

	bool _lookup(uint8_t &out, uint32_t in) {
		for (const auto &e : lookup) {
			if (e.uni <= in && in < e.uni + e.range) {
				out = e.asc + (in - e.uni);
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
					cur = 0;
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
