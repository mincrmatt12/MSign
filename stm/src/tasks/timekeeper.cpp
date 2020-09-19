#include "timekeeper.h"
#include "../srv.h"
#include "../common/slots.h"

extern srv::Servicer servicer;

void tasks::Timekeeper::systick_handler() {
	this->current_time += 1;
	this->timestamp += 1;
}

void tasks::Timekeeper::loop() {
	srv::ServicerLockGuard g(servicer);

	if ((current_time - last_run_time) > 30000 || first) {
		servicer.set_temperature(slots::TIME_OF_DAY, bheap::Block::TemperatureHot);
	}
	if (servicer.slot_dirty(slots::TIME_OF_DAY)) {
		servicer.set_temperature(slots::TIME_OF_DAY, bheap::Block::TemperatureWarm);
		this->last_run_time = this->current_time;
		this->timestamp = servicer.slot<uint64_t>(slots::TIME_OF_DAY);
		this->first = false;
	}
}
