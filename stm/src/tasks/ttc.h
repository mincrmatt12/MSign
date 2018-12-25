#ifndef TTC_H
#define TTC_H

#include "sched.h"
#include <stdint.h>

namespace tasks {
	struct TTCScreen : public sched::Task, sched::Screen {
		void init() override {};
		void deinit() override {};

		void loop() override;
		bool done() override {return true;}
	};
}

#endif
