#include "timekeeper.h"
#include "../srv.h"
#include "../common/slots.h"

extern srv::Servicer servicer;

void tasks::Timekeeper::systick_handler() {
	this->current_time += 1;
	this->timestamp += 1;
}

void tasks::Timekeeper::loop() {
	if ((current_time - last_run_time) > 30000 || first) {
		uint64_t time_when_requested, current_timestamp;

		switch (servicer.request_time(current_timestamp, time_when_requested)) {
			case slots::protocol::TimeStatus::Ok:
				timestamp = (this->current_time - time_when_requested) + current_timestamp;
				last_run_time = current_time;
				first = false;
			case slots::protocol::TimeStatus::NotSet:
				vTaskDelay(1000);
			default:
				break;
		}
	}
}
