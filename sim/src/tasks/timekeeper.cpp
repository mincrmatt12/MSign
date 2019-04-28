#include "timekeeper.h"

#include "srv.h"
#include "common/slots.h"
#include <sys/time.h>

extern srv::Servicer servicer;

bool tasks::Timekeeper::important() {
	return true;
}

bool tasks::Timekeeper::done() {
	return true; // again, only runs for one cycle.
}

void tasks::Timekeeper::systick_handler() {
	this->current_time += 1;
	this->timestamp += 1;
}

void tasks::Timekeeper::loop() {
	timeval current;
	gettimeofday(&current, nullptr);

	current.tv_usec /= 1000;
	current.tv_usec += current.tv_sec * 1000;

	this->current_time += current.tv_usec - last_run_time ;
	this->timestamp = current.tv_usec;

	last_run_time = this->current_time;
}
