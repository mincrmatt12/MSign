#ifndef MSE_DEBUG_H
#define MSE_DEBUG_H

#include <stdint.h>

namespace debug {
	// Debug console: listens over websockets and implements a stupid command line interface

	void init();
	void loop();

	// Takes in the arguments; followed by a pointer reference? WTF? nah, you're supposed to increment it to mark the end.
	// It's the simplest way i could think of: buffer += snprintf(buffer, end - buffer, "asdfasdfasdf%d", 123);
	// End marks the end of the region you can print to.
	// this is usually allocated as a 512 byte region.
	typedef void (*CommandHandler)(const char * args, char *&buffer, const char *end);
	void add_command(const char *prefix, CommandHandler ch, int outputSize=512);

	// Send a message out-of-band to the debug console.
	void send_msg(char *buffer, int amt);
}

#endif
