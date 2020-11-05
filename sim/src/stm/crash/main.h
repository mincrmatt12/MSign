#ifndef MSN_SIM_CRASH_H
#define MSN_SIM_CRASH_H

namespace crash {
	[[noreturn]] void panic(const char* errcode);
	[[noreturn]] void panic_nonfatal(const char* errcode);
};

#endif
