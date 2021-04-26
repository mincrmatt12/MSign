#include "crash/main.h"
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

matrix_type matrix __attribute__((section(".vram")));
srv::Servicer servicer{};
uint64_t rtc_time;

bool finished_init_ok = false;

tasks::Timekeeper   timekeeper{rtc_time};
tasks::DispMan      dispman{};
tasks::DebugConsole dbgtim{timekeeper};

void out_of_memory() {
	matrix.start_display();
	crash::panic("no memory for tasks");
}

int main() {
	rcc::init();
	nvic::init();
	rng::init();

	matrix.init();
	servicer.init();

	if (xTaskCreate([](void *arg){((srv::Servicer *)arg)->run();}, "srvc", 256, &servicer, 5, nullptr) != pdPASS) out_of_memory();
	if (xTaskCreate([](void *arg){((tasks::DispMan *)arg)->run();}, "screen", 512, &dispman, 4, nullptr) != pdPASS) out_of_memory();
	if (xTaskCreate([](void *arg){((tasks::DebugConsole *)arg)->run();}, "dbgtim", 176, &dbgtim, 2, nullptr) != pdPASS) out_of_memory();

	matrix.start_display();
	finished_init_ok = true;
	vTaskStartScheduler();
}
