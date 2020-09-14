#include "timekeeper.h"
#include "../srv.h"
#include "../common/slots.h"

extern srv::Servicer servicer;

void tasks::Timekeeper::systick_handler() {
	this->current_time += 1;
	this->timestamp += 1;
}

void tasks::Timekeeper::loop() {
	if (last_run_time == 0) {
		// Init temperature of TIME to warm
		servicer.set_temperature(slots::TIME_OF_DAY, bheap::Block::TemperatureWarm);
	}
	if ((current_time - last_run_time) > 30000 || last_run_time <= 2) {
		servicer.set_temperature(slots::TIME_OF_DAY, bheap::Block::TemperatureHot);
	}
	if (servicer.slot_dirty(slots::TIME_OF_DAY)) {
		servicer.set_temperature(slots::TIME_OF_DAY, bheap::Block::TemperatureWarm);
		this->last_run_time = this->current_time;
		this->timestamp = servicer.slot<uint64_t>(slots::TIME_OF_DAY);
	}
}
