#include "debug.h"
#include "srv.h"
#include <string.h>
#include <stdio.h>

extern sched::TaskPtr task_list[8];
extern srv::ConIO debug_in, debug_out;

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
		char buffer[128];
		memset(buffer, 0, 128);

		// header: 
		// 0 - 4\n
		// 1 + 3 + 4 + 1
		// 9
		for (int i = 0; i < 8; ++i) {
			if (task_list[i]) 
				sprintf(buffer + (i * 9), "%1d - %.4s\n", i, task_list[i]->name);
			else
				sprintf(buffer + (i * 9), "%1d -     \n", i);
		}

		debug_out.write(buffer, strlen(buffer));
	}
	else {
		debug_out.write("invalid command\n", 16);
	}
}
