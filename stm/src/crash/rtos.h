#ifndef MSN_CRASH_RTOS_H
#define MSN_CRASH_RTOS_H

#include "FreeRTOS.h"
#include <stdint.h>
#include <concepts>

namespace crash::rtos {
	// Try to figure out the active thread name _safely_, otherwise return null.
	const char * guess_active_thread_name();

	// Is the RTOS active?
	bool is_rtos_running();

	void get_task_regs(uint32_t *topofstack, uint32_t& PC, uint32_t& SP, uint32_t &LR);

	// Read queue contents for diagnostic purposes
	template<typename ItemType, typename Func>
		requires std::is_trivial_v<ItemType>
	bool for_all_in_queue(const StaticQueue_t *qt, Func&& f) {
		// based on queue.c
		uint8_t *pcHead = (uint8_t *)qt->pvDummy1[0],
				*pcTail = (uint8_t *)qt->pvDummy1[2],
				*pcReadFrom = (uint8_t *)qt->u.pvDummy2;
		UBaseType_t uxItems = qt->uxDummy4[0];

		if (sizeof(ItemType) != qt->uxDummy4[2]) return false;
		if (uxItems > qt->uxDummy4[1]) return false;

		ItemType it;

		while (uxItems--) {
			pcReadFrom += sizeof(ItemType);
			if (pcReadFrom >= pcTail) pcReadFrom = pcHead;
			memcpy(&it, pcReadFrom, sizeof(ItemType));
			f(it);
		}

		return true;
	}
}

#endif
