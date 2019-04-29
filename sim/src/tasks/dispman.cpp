#include "tasks/timekeeper.h"
#include "dispman.h"
#include <iostream>

extern tasks::Timekeeper timekeeper;
extern sched::TaskPtr task_list[8];

void tasks::DispMan::init() {
	this->on = 0;
	threedbg.init();
	clockfg.init();
	activate(0);
}

void tasks::DispMan::loop() {
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
	task_list[0] = &threedbg;
	task_list[1] = &clockfg;
}
