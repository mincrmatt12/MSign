#ifndef MSN_UTIL_H
#define MSN_UTIL_H

#include <stddef.h>
#include <stdint.h>

namespace util {
	uint16_t compute_crc(uint8_t * buffer, size_t length, uint16_t crc_previous=0);

	bool crc_valid(uint8_t * buffer, size_t length_including_crc);
};

#endif
