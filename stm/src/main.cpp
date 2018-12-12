#include "stm32f2xx.h"
#include "nvic.h"
#include "rcc.h"
#include "rng.h"
#include "fonts/latob_15.h"
#include "fonts/latob_12.h"
#include "fonts/tahoma_9.h"
#include "fonts/dejavu_10.h"
#include "fonts/vera_7.h"
#include "sched.h"

// strange parse error - put this last...

#include "draw.h"
#include <stdlib.h>
#include <cmath>

#define SKIP_THRESH 5

led::Matrix<led::FrameBuffer<64, 32>> matrix;

// Scheduler parameters

// Slots 0-2: display tasks; need to be run before swap_buffers
// Slot  3  : the service interface - only marks as important when data is in buffer; talks to esp
// Slots 4-7: low priority stuff irrelevant of the thing; possibly alarms/etc, sometimes timekeeping
//
// Slot 0 is usually the active display
// Slot 1 is usually the switcher, responsible for managing slot 0
// Slot 2 is an overlay, which can be started in response to a notification
sched::TaskPtr tasks[8] = {nullptr};
uint8_t task_index = 0;

// increments per skipped task
uint8_t skipped_counter[8] = {0};

// true when the display is ready
bool    display_ready = false;

// counter since display was ready
uint16_t display_counter = 0;

int main() {
	rcc::init();
	nvic::init();
	rng::init();
	matrix.init();

	// .... TODO: INIT esp comms, should be dispalying some test pattern while this occurs .......

	// Main loop of software
	while (true) {
		matrix.display();
		// New frame is being displayed; is everything finished?
		if (task_index >= 8) {
			task_index = 0;
		}
		// ... scheduler loop ...
		while (matrix.is_active()) {
			if (task_index >= 8) {
				if (display_ready) continue;
				task_index = 0;
			}

			// Check if the current task is null, if so skip
			
			if (tasks[task_index] == nullptr) {
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
					if (tasks[task_index]->important()) goto run_it;
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

			tasks[task_index]->loop();
			if (tasks[task_index]->done()) {
				++task_index;
				if (task_index == 3) display_ready = true;
			}
		}

		if (display_ready) {
			matrix.swap_buffers(); 
			display_ready = false;
		}
	}
}

extern "C" void DMA2_Stream5_IRQHandler() {
	if (LL_DMA_IsActiveFlag_TC5(DMA2)) {
		LL_DMA_ClearFlag_TC5(DMA2);
		matrix.dma_finish();
	}
}

extern "C" void TIM1_BRK_TIM9_IRQHandler() {
	if (LL_TIM_IsActiveFlag_UPDATE(TIM9)) {
		LL_TIM_ClearFlag_UPDATE(TIM9);
		matrix.tim_elapsed();
	}
}
