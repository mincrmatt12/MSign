#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "tasks/timekeeper.h"
#include "tasks/screen.h"
#include "tasks/debug.h"
#include "srv.h"

// strange parse error - put this last...

#include "draw.h"
#include <stdlib.h>
#include <cmath>
#include <iostream>
#include <time.h>
#include <sys/time.h>

matrix_type matrix;

srv::Servicer servicer;
uint64_t rtc_time;

tasks::Timekeeper timekeeper(rtc_time);
tasks::DispMan    dispman{};
tasks::DebugConsole dbgtim{timekeeper};

extern void pump_faux_dma_task(void*);
char ** orig_argv;

int main(int argc, char ** argv) {
	orig_argv = argv;
	rcc::init();
	nvic::init();
	rng::init();
	matrix.init();
	std::cout.tie(0);
	servicer.init();
	
	xTaskCreate((TaskFunction_t)&srv::Servicer::run, "srvc", 256, &servicer, 5, nullptr);
	xTaskCreate((TaskFunction_t)&tasks::DispMan::run, "screen", 512, &dispman, 4, nullptr);
	xTaskCreate((TaskFunction_t)&tasks::DebugConsole::run, "dbgtim", 176, &dbgtim, 2, nullptr);
	xTaskCreate(pump_faux_dma_task, "faux", 1024, nullptr, 6, nullptr);

	matrix.start_display();
	vTaskStartScheduler();
}
