#include "rcc.h"
#include "tskmem.h"
#include "regman.h"

#include <FreeRTOS.h>
#include <task.h>

void main() {
	rcc::init();

	tskmem::regman.create(regman::man, "regman", 1);

	vTaskStartScheduler();
}
