#include "timekeeper.h"
#include <stm32f2xx.h>

#include "srv.h"
#include "common/slots.h"

extern srv::Servicer servicer;

uint32_t last_ticks_value = 0;

bool tasks::Timekeeper::important() {
	return (current_time - last_run_time) > 30000 || last_run_time <= 2;
}

bool tasks::Timekeeper::done() {
	return true; // again, only runs for one cycle.
}

void tasks::Timekeeper::systick_handler() {
	uint32_t incr_amt = SysTick->VAL - last_ticks_value;
	last_ticks_value = SysTick->VAL;

	this->current_time += incr_amt;
	this->timestamp += incr_amt;
}

void tasks::Timekeeper::loop() {
	if (last_run_time == 0) {
		if (servicer.open_slot(slots::TIME_OF_DAY, false, this->time_slot, true)) {
			last_run_time += 1;
		}

		name[0] = 't';
		name[1] = 'i';
		name[2] = 'm';
		name[3] = 'e';
	}
	else if (last_run_time == 1) {
		if (servicer.slot_connected(this->time_slot)) {
			this->last_run_time += 1;
		}
	}
	else if (important()) {
		if (!waiting_for_data) {
			waiting_for_data = true;
			servicer.ack_slot(this->time_slot);
			ack_timer = 0;
		}
		else if (servicer.slot_dirty(this->time_slot, true)) {
			waiting_for_data = false;
			this->last_run_time = this->current_time;
			this->timestamp = *(uint64_t *)(servicer.slot(this->time_slot));
		}
		else {
			++ack_timer;
			if (ack_timer > 100) {
				waiting_for_data = false;
			}
		}
	}
}
