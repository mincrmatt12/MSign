#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "tasks/timekeeper.h"
#include "tasks/screen.h"
#include "tasks/debug.h"
#include "srv.h"
#include "tskmem.h"

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

tskmem::TaskHolder<1024> faux_dma_task;

int main(int argc, char ** argv) {
	orig_argv = argv;
	rcc::init();
	nvic::init();
	rng::init();
	matrix.init();
	std::cout.tie(0);
	servicer.init();
	
	tskmem::srvc.create(servicer, "srvc", 5);
	tskmem::screen.create(dispman, "srvc", 4);
	tskmem::dbgtim.create(dbgtim, "dbtim", 2);
	faux_dma_task.create(pump_faux_dma_task, "faux", nullptr, 6);

	matrix.start_display();
	vTaskStartScheduler();
}
