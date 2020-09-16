#include "stm32f2xx.h"
#include "stm32f2xx_ll_rcc.h"
#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "fonts/latob_15.h"
#include "fonts/dejavu_10.h"
#include "fonts/lcdpixel_6.h"
#include "tasks/timekeeper.h"
#include "srv.h"
#include "common/bootcmd.h"

// strange parse error - put this last...

#include "draw.h"
#include <cstring>
#include <stdlib.h>
#include <cmath>

#define SKIP_THRESH 5

#ifndef MSIGN_GIT_REV
#define MSIGN_GIT_REV
#endif

matrix_type matrix;

srv::Servicer servicer{};
uint64_t rtc_time;

tasks::Timekeeper timekeeper(rtc_time);

template <typename FB>
void show_test_pattern(uint8_t stage, FB& fb, const char * extra=nullptr) {
	fb.clear();
	draw::text(fb, "MSIGN V3.1" MSIGN_GIT_REV, font::lcdpixel_6::info, 0, 7, 0, 4095, 0);
	draw::text(fb, "STM OK", font::lcdpixel_6::info, 0, 21, 4095, 4095, 4095);
	char buf[5] = {0};
	strncpy(buf, bootcmd_get_bl_revision(), 4);
	draw::text(fb, buf, font::lcdpixel_6::info, draw::text(fb, "BLOAD ", font::lcdpixel_6::info, 0, 14, 4095, 127_c, 0), 14, 4095, 4095, 4095);
	switch (stage) {
		case 1:
			draw::text(fb, "ESP WAIT", font::lcdpixel_6::info, 0, 28, 4095, 0, 0);
			break;
		case 2:
			draw::text(fb, "ESP OK", font::lcdpixel_6::info, 0, 28, 4095, 4095, 4095);
			draw::text(fb, "UPD NONE", font::lcdpixel_6::info, 0, 35, 0, 4095, 0);
			break;
		case 3:
			draw::text(fb, "ESP OK", font::lcdpixel_6::info, 0, 28, 4095, 4095, 4095);
			if (extra) {
				draw::text(fb, extra, font::lcdpixel_6::info, 0, 35, 40_c, 40_c, 4095);
			}
			else {
				draw::text(fb, "UPD INIT", font::lcdpixel_6::info, 0, 35, 40_c, 40_c, 4095);
			}
			break;
		default:
			break;
	}
}

void screen_task(void *) {
	show_test_pattern(0, matrix.get_inactive_buffer());

	matrix.swap_buffers();

	// Init esp comms
	while (!servicer.ready()) {
		show_test_pattern(1, matrix.get_inactive_buffer());
		matrix.swap_buffers();
	}

	if (servicer.updating()) {
		// Go into a simple servicer only update mode
		while (true) {
			show_test_pattern(3, matrix.get_inactive_buffer(), servicer.update_status());
			matrix.swap_buffers();
		}

		// Servicer will eventually reset the STM.
	}

	show_test_pattern(2, matrix.get_inactive_buffer());
	matrix.swap_buffers();
	show_test_pattern(2, matrix.get_inactive_buffer());
	matrix.swap_buffers();

	uint16_t finalize_counter = 60;
	while (finalize_counter--) {
		if (finalize_counter >= 50) matrix.swap_buffers();
		else if (finalize_counter >= 34) {
			draw::fill(matrix.get_inactive_buffer(), 4095, 0, 0);
			matrix.swap_buffers();
		}
		else if (finalize_counter >= 18) {
			draw::fill(matrix.get_inactive_buffer(), 0, 4095, 0);
			matrix.swap_buffers();
		}
		else {
			draw::fill(matrix.get_inactive_buffer(), 0, 0, 4095);
			matrix.swap_buffers();
		}
	}

	while (1) {
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}

int main() {
	rcc::init();
	nvic::init();
	rng::init();

	matrix.init();
	servicer.init();

	xTaskCreate((TaskFunction_t)&srv::Servicer::run, "srvc", 256, &servicer, 5, nullptr);
	xTaskCreate(screen_task, "screen", 512, nullptr, 4, nullptr);

	matrix.start_display();
	vTaskStartScheduler();
}
