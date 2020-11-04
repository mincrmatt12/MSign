#include "rtos.h"

#include <FreeRTOS.h>
#include <task.h>

extern bool finished_init_ok;
extern "C" uint8_t _ecstack, _estack;

bool crash::rtos::is_rtos_running() {
	return finished_init_ok;
}

const char* crash::rtos::guess_active_thread_name() {
	// Try and get a TCB pointer
	void * tcb = xTaskGetCurrentTaskHandle();
	// Make sure this pointer makes sense
	if (tcb < &_ecstack || tcb >= &_estack) {
		return nullptr;
	}
	return pcTaskGetName(NULL);
}
