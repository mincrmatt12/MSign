#include "crash/main.h"
#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "tasks/debug.h"
#include "tasks/timekeeper.h"
#include "tasks/screen.h"
#include "srv.h"
#include "tskmem.h"

// strange parse error - put this last...

#include "draw.h"
#include <cstring>
#include <stdlib.h>
#include <cmath>

matrix_type matrix __attribute__((section(".vram")));
// Hackery to convince gcc to put this in the right section
template RAMFUNC void matrix_type::framebuffer_type::prepare_stream(uint16_t i, uint8_t pos, uint8_t * bs);
srv::Servicer servicer{};
uint64_t rtc_time;

bool finished_init_ok = false;

tasks::Timekeeper   timekeeper{rtc_time};
tasks::DispMan      dispman{};
tasks::DebugConsole dbgtim{timekeeper};

int main() {
	rcc::init();
	nvic::init();
	rng::init();

	matrix.init();
	servicer.init();

	tskmem::srvc.create(servicer, "srvc", 5);
	tskmem::screen.create(dispman, "screen", 4);
	tskmem::dbgtim.create(dbgtim, "dbtim", 2);

	matrix.start_display();
	finished_init_ok = true;
	vTaskStartScheduler();
}
