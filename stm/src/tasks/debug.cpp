#include "debug.h"
#include "screen.h"
#include "../srv.h"
#include <string.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>

// massive list of crap we hack at
extern tasks::DispMan dispman;

void tasks::DebugConsole::run() {
	while (true) {
		// TODO: debug stuff

		// rn, just run the timekeeper
		tim.loop();
		vTaskDelay(5000);
	}
}
