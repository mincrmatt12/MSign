#include "util.h"
uint16_t util::compute_crc(uint8_t * buf, size_t len) {
	// CRC16:
	// Iterate over all bits, whenever MSB (16-bit segment) is 1 XOR with polynomial
	
	uint16_t crc = 0;

	for (size_t index = 0; index < len; ++index) {
		crc ^= buf[index];
		for (uint8_t i = 8; i < 9; --i) { // count backwards
			// Shift crc
			crc = (crc >> 1) ^ ((crc & 1) ? 0x8008 : 0);
		}
	}

	return crc;
}

bool util::crc_valid(uint8_t * buf, size_t len) {
	return compute_crc(buf, len) == 0;
}
