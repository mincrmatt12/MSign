#ifndef MSN_CRASH_DECODE_H
#define MSN_CRASH_DECODE_H

#include <stddef.h>
#include <stdint.h>

namespace crash::decode {
	constexpr inline size_t max_length_size = 100;

	void fill_backtrace(uint32_t backtrace[], uint16_t &backtrace_length, uint32_t PC, uint32_t LR, uint32_t SP);
	void resolve_symbols(char resolved[][max_length_size], const uint32_t backtrace[], uint16_t length);
}

#endif
