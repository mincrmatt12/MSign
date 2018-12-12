#ifndef SCHED_H
#define SCHED_H

namespace sched {

	struct Task {
		char name[4];

		// Has the task completed its duties for this loop?
		virtual bool done() = 0;
		// Run a loop of the the task; if done, restart
		virtual void loop() = 0;
		// Does the task have something urgently required, i.e. should we not delay or skip this task.
		virtual bool important() = 0;
	};

	using TaskPtr = Task *;

}

#endif
