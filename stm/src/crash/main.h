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
	[[noreturn]] __attribute__((always_inline)) void inline panic(const char* errcode) {
		uint32_t pc, lr, sp;
		asm volatile (
			"mov %0, pc\n\t"
			"mov %1, lr\n\t"
			"mov %2, sp\n\t" :
			"=r" (pc), "=r" (lr), "=r" (sp)
		);
		call_crash_main(errcode, sp, pc, lr);
	}
};

#endif
