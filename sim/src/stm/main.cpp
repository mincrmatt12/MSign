#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "fonts/latob_15.h"
#include "fonts/dejavu_10.h"
#include "fonts/lcdpixel_6.h"
#include "srv.h"
#include "tasks/timekeeper.h"

// strange parse error - put this last...

#include "draw.h"
#include <stdlib.h>
#include <cmath>
#include <iostream>
#include <time.h>
#include <sys/time.h>

#define SKIP_THRESH 5
matrix_type matrix;

srv::Servicer servicer;
uint64_t rtc_time;

tasks::Timekeeper timekeeper{rtc_time};

extern void pump_faux_dma_task(void*);

template <typename FB>
void show_test_pattern(uint8_t stage, FB& fb, const char * extra=nullptr) {
	draw::fill(fb, 0, 0, 0);
	draw::text(fb, "MSIGN .. V2", font::lcdpixel_6::info, 0, 7, 0, 255_c, 0);
	draw::text(fb, "STM .. OK", font::lcdpixel_6::info, 0, 14, 255_c, 255_c, 255_c);
	switch (stage) {
		case 1:
			draw::text(fb, "ESP .. WAIT", font::lcdpixel_6::info, 0, 21, 255_c, 0, 0);
			break;
		case 2:
			draw::text(fb, "ESP .. OK", font::lcdpixel_6::info, 0, 21, 255_c, 255_c, 255_c);
			draw::text(fb, "UPD .. NONE", font::lcdpixel_6::info, 0, 28, 0, 255_c, 0);
			break;
		case 3:
			draw::text(fb, "ESP .. OK", font::lcdpixel_6::info, 0, 21, 255_c, 255_c, 255_c);
			if (extra) {
				draw::text(fb, extra, font::lcdpixel_6::info, 0, 28, 40, 40, 255_c);
			}
			else {
				draw::text(fb, "UPD .. INIT", font::lcdpixel_6::info, 0, 28, 40, 40, 255_c);
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
	xTaskCreate(screen_task, "screen", 512, nullptr, 4, nullptr);
	xTaskCreate(pump_faux_dma_task, "faux", 1024, nullptr, 6, nullptr);

	matrix.start_display();
	vTaskStartScheduler();
}
