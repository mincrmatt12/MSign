#ifndef SCHEDEF_H
#define SCHEDEF_H

namespace sched {

	struct Task {
		char name[4];

		// Has the task completed its duties for this loop?
		virtual bool done() = 0;
		// Run a loop of the the task; if done, restart
		virtual void loop() = 0;
		// Does the task have something urgently required, i.e. should we not delay or skip this task.
		// All tasks always try to run, so use this is data is pending.
		virtual bool important() {return false;}
	};

	using TaskPtr = Task *;

	struct Screen {
		virtual bool init() {return true;}

		virtual bool deinit() {return true;}
	};

}

#endif
