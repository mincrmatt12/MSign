#ifndef MSN_STCRASH_MAIN_H
#define MSN_STCRASH_MAIN_H

#include <stdint.h>

namespace crash {
	// Transfer control to the crash handler.
	//
	// `errcode` must either be null or point to memory outside of the crash handler's region (i.e.
	// outside VRAM)
	//
	// This function does not return.
	[[noreturn]] void call_crash_main(const char *errcode, uint32_t SP, uint32_t PC, uint32_t LR) __attribute__((naked));

	// Transfer control to the crash handler from an arbitrary point in the code
	[[noreturn]] void panic(const char *msg);
};

#endif
