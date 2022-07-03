#ifndef MSN_SIM_CRASH_H
#define MSN_SIM_CRASH_H

namespace crash {
	[[noreturn]] void panic(const char* errcode);
	[[noreturn]] void panic_nonfatal(const char* errcode);
};

extern "C" void msign_panic(const char *c);

inline void msign_assert(bool c, const char * v) {
	if (!c) msign_panic(v);
}

#endif
