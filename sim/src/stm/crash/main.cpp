#include "main.h"
#include <cstdio>
#include <FreeRTOS.h>
#include <cstdlib>
#include <task.h>

namespace crash {
	[[noreturn]] void panic(const char* errcode) {
		vTaskSuspendAll();

		puts("");
		puts("");
		puts("PANIC: ");
		puts(errcode);

		exit(1);
	}

	[[noreturn]] void panic_nonfatal(const char* errcode) {
		vTaskSuspendAll();

		puts("");
		puts("");
		puts("PANIC (nonfatal): ");
		puts(errcode);

		exit(2);
	}
}

extern "C" void msign_panic(const char *c) {
	crash::panic(c);
}
