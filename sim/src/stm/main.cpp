#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "fonts/latob_15.h"
#include "fonts/dejavu_10.h"
#include "fonts/lcdpixel_6.h"
#include "schedef.h"
#include "srv.h"
#include "tasks/timekeeper.h"

// strange parse error - put this last...

#include "draw.h"
#include "tasks/dispman.h"
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

extern void pump_faux_dma();

tasks::DispMan dispman;

// Scheduler parameters

// Slots 0-2: display tasks; need to be run before swap_buffers
// Slot  3  : the service interface - only marks as important when data is in buffer; talks to esp
// Slots 4-7: low priority stuff irrelevant of the thing; possibly alarms/etc, sometimes timekeeping
//
// Slot 0 is usually the active display
// Slot 1 is usually the switcher, responsible for managing slot 0
// Slot 2 is an overlay, which can be started in response to a notification
sched::TaskPtr task_list[8] = {nullptr};
uint8_t task_index = 0;

// increments per skipped task, used to make sure task_list run on time
uint8_t skipped_counter[8] = {0};

// true when the display is ready
bool    display_ready = false;

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

char ** orig_argv;

int main(int argc, char ** argv) {
	orig_argv = argv;
	rcc::init();
	nvic::init();
	rng::init();
	matrix.init();

	std::cout.tie(0);

	servicer.init();
	task_list[3] = &servicer;
	task_list[4] = &dispman;
	task_list[5] = &timekeeper;

	show_test_pattern(0, matrix.get_inactive_buffer());
	matrix.swap_buffers();

	// Init esp comms
	while (!servicer.ready()) {
		pump_faux_dma();
		servicer.loop();
		matrix.display();
		while (matrix.is_active()) {;}
		show_test_pattern(1, matrix.get_inactive_buffer());
		matrix.swap_buffers();
	}

	if (servicer.updating()) {
		// Go into a simple servicer only update mode
		while (true) {
			pump_faux_dma();
			servicer.loop();
			matrix.display();
			show_test_pattern(3, matrix.get_inactive_buffer(), servicer.update_status());
			while (matrix.is_active()) {pump_faux_dma(); servicer.loop();}
			matrix.swap_buffers();
		}

		// Servicer will eventually reset the STM.
	}

	show_test_pattern(2, matrix.get_inactive_buffer());
	matrix.swap_buffers();
	show_test_pattern(2, matrix.get_inactive_buffer());
	matrix.swap_buffers();

	uint16_t finalize_counter = 120;
	while (finalize_counter--) {
		if (finalize_counter < 50) {
			draw::fill(matrix.get_inactive_buffer(), 255_c, 0, 0);
		}
		if (finalize_counter < 34) {
			draw::fill(matrix.get_inactive_buffer(), 0, 255_c, 0);
		}
		if (finalize_counter < 18) {
			draw::fill(matrix.get_inactive_buffer(), 0, 0, 255_c);
		}
		matrix.swap_buffers();
		matrix.display();
		while (matrix.is_active()) {;}
	}

	dispman.init();

	auto to_millis = [](const timeval& x){return (x.tv_sec * 1000 + x.tv_usec / 1000);};

	timeval last_systick;
	gettimeofday(&last_systick, NULL);

	// Main loop of software
	while (true) {
		matrix.display();
		// ... scheduler loop ...
		while (matrix.is_active()) {
			// Update fake DMA
			pump_faux_dma();
			//simulator time
			timeval current_time;
			gettimeofday(&current_time, NULL);
			for (int i = 0; i < (to_millis(current_time) - to_millis(last_systick)); ++i) {
				timekeeper.systick_handler();                             
			}
			last_systick = current_time;

			// Are we done?
			if (task_index >= 8) {
				if (display_ready) continue;
				// No, we are starting anew
				task_index = 0;
			}

			// Check if the current task is null, if so skip

			if (task_list[task_index] == nullptr) {
				++task_index;
				if (task_index == 3) display_ready = true;
				continue;
			}

			// Are we in a display slot?

			if (task_index <= 2) {
				// If so, then we need to treat the task as important
				goto run_it;
			}
			else {
				if (!display_ready) {
					// we are running on borrowed time, skip unless important
					if (task_list[task_index]->important()) goto run_it;
					goto skip;
				}
				else {
					// run the task, it is ok
					goto run_it;
				}
			}

skip:
			++skipped_counter[task_index];
			if (skipped_counter[task_index] > SKIP_THRESH) {
				--skipped_counter[task_index];
				goto run_it;
			}
			++task_index;
			continue;
run_it:
			if (skipped_counter[task_index] > 0)
				--skipped_counter[task_index];

			task_list[task_index]->loop();
			if (task_list[task_index]->done()) {
				++task_index;
				if (task_index == 3) display_ready = true;
			}
		}

		if (display_ready) {
			matrix.swap_buffers(); 
			draw::fill(matrix.get_inactive_buffer(), 0, 0, 0);
			display_ready = false;
			task_index = 0;
		}
	}
}
