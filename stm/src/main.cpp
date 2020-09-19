#include "stm32f2xx.h"
#include "stm32f2xx_ll_rcc.h"
#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "tasks/debug.h"
#include "tasks/timekeeper.h"
#include "tasks/screen.h"
#include "srv.h"

// strange parse error - put this last...

#include "draw.h"
#include <cstring>
#include <stdlib.h>
#include <cmath>

matrix_type matrix;
srv::Servicer servicer{};
uint64_t rtc_time;

tasks::Timekeeper   timekeeper{rtc_time};
tasks::DispMan      dispman{};
tasks::DebugConsole dbgtim{timekeeper};

int main() {
	rcc::init();
	nvic::init();
	rng::init();

	matrix.init();
	servicer.init();

	xTaskCreate((TaskFunction_t)&srv::Servicer::run, "srvc", 256, &servicer, 5, nullptr);
	xTaskCreate((TaskFunction_t)&tasks::DispMan::run, "screen", 512, &dispman, 4, nullptr);
	xTaskCreate((TaskFunction_t)&tasks::DebugConsole::run, "dbgtim", 176, &dbgtim, 2, nullptr);

	matrix.start_display();
	vTaskStartScheduler();
}
