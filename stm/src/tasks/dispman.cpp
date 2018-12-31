#include "tasks/timekeeper.h"
#include "dispman.h"

extern tasks::Timekeeper timekeeper;
extern sched::TaskPtr task_list[8];

void tasks::DispMan::init() {
	setup(0);
	activate(0);
	setup(1);
	this->on = 0;
}

void tasks::DispMan::loop() {
	if (timekeeper.current_time - this->last_screen_transition > 12000) {
		teardown(this->on);
		++this->on;
		this->on %= this->count;
		activate(this->on);
		setup((this->on + 1) % this->count);
		last_screen_transition = timekeeper.current_time;
	}
}

void tasks::DispMan::setup(uint8_t i) {
	switch (i) {
		case 0:
			ttc.init();
			break;
		case 1:
			weather.init();
			break;
		case 2:
			threedbg.init();
			clockfg.init();
			break;
	}
}

void tasks::DispMan::teardown(uint8_t i) {
	switch (i) {
		case 0:
			ttc.deinit();
			break;
		case 1:
			weather.deinit();
			break;
		case 2:
			threedbg.deinit();
			clockfg.deinit();

			task_list[1] = nullptr;

			break;
	}
}

void tasks::DispMan::activate(uint8_t i) {
	switch (i) {
		case 0:
			task_list[0] = &ttc;
			break;
		case 1:
			task_list[0] = &weather;
			break;
		case 2:
			task_list[0] = &threedbg;
			task_list[1] = &clockfg;
			break;
	}
}
