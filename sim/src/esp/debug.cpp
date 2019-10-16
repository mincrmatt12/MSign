#include "debug.h"

namespace debug {
	void init() {};
	void loop() {};

	void add_command(const char *prefix, CommandHandler ch, int outputSize) {};

	void send_msg(char *buffer, int amt) {
		Serial1.print("(mock) dseng_msg: ");
		Serial1.write(buffer, amt);
		Serial1.println();
	};
}
