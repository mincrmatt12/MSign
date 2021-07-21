#ifndef MSN_CRASH_RTOS_H
#define MSN_CRASH_RTOS_H

#include <stdint.h>

namespace crash::rtos {
	// Try to figure out the active thread name _safely_, otherwise return null.
	const char * guess_active_thread_name();

	// Is the RTOS active?
	bool is_rtos_running();

	void get_task_regs(uint32_t *topofstack, uint32_t& PC, uint32_t& SP, uint32_t &LR);
}

#endif
