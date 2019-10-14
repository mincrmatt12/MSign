#include "debug.h"
#include "srv.h"
#include <string.h>
#include <stdio.h>

#include "dispman.h"

extern sched::TaskPtr task_list[8];
extern srv::ConIO debug_in, debug_out;

// massive list of crap we hack at
extern tasks::DispMan dispman;

bool tasks::DebugConsole::important() {
	return debug_in.remaining();
}

bool tasks::DebugConsole::done() {return important();}

void tasks::DebugConsole::loop() {
	if (debug_in.remaining() == 0) return;
	char cmdbuf[debug_in.remaining() + 1];
	cmdbuf[debug_in.remaining()] = 0;

	debug_in.read(cmdbuf, debug_in.remaining());

	// Check command.
	
	if (strncmp(cmdbuf, "ping", 4) == 0) {
		debug_out.write("pong!\n", 6);
	}
	else if (strncmp(cmdbuf, "tasks", 5) == 0) {
		// header: 
		// 0 - 4\n
		// 1 + 3 + 4 + 1
		// 9
		for (int i = 0; i < 8; ++i) {
			if (task_list[i]) 
				fprintf(stderr, "%1d - %4.4s\n", i, task_list[i]->name);
			else
				fprintf(stderr, "%1d -     \n", i);
		}
	}
	else if (strncmp(cmdbuf, "adv", 3) == 0) {
		debug_out.write("advancing\n", 10);

		dispman.force_advance();
	}
	else if (strncmp(cmdbuf, "hold ", 5) == 0) {
		dispman.set_hold(cmdbuf[5] == 'e');
	}
	else {
		debug_out.write("invalid command\n", 16);
	}
}
