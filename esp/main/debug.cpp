/*
#include "debug.h"
#include "util.h"
#include "serial.h"

namespace debug {
	//WebSocketsServer wss(81);
	int connected = -1;
	// 0 - unk
	// 1 - log
	// 2 - dbg
	int is_log = 0;

	int num_of_commands = 0;
	CommandHandler command_handlers[20];
	int            allocation_sizes[20];
	const char *   command_prefixes[20];

	int match_command(char * in, char *& out) {
		for (int i = 0; i < num_of_commands; ++i) {
			if (strstr(in, command_prefixes[i]) == in) {
				out = in + strlen(command_prefixes[i]);
				return i;
			}
		}
		return -1;
	}

	void logger_hook(uint8_t *buf, size_t length) {
		//wss.broadcastTXT(buf, length);
	}

	void init() {
		//wss.begin();
		//wss.onEvent(ws_event);

		// commands
		add_command("fheap", [](const char *args, char *&begin, const char *){
			begin += sprintf_P(begin, PSTR("free heap: %d\n"), ESP.getFreeHeap());
		}, 32);
		add_command("shutup", [](const char *args, char *&begin, const char *){
			if (args[1] == 'e') Log.quiet_mode = true;
			if (args[1] == 'd') Log.quiet_mode = false;
		}, 8);
		add_command("stm", [](const char *args, char *&begin, const char*){
			if (args[0] == ' ') ++args;

			serial::interface.send_console_data((const uint8_t *)args, strlen(args));
		}, 8);
		add_command("help", [](const char *, char *&buffer, const char *end) {
			for (int i = 0; i < num_of_commands; ++i) {
				buffer += snprintf_P(buffer, end - buffer, PSTR("- %s\n"), command_prefixes[i]);
			}
		}, 64);
	}

	void loop() {
		//wss.loop();
	}

	void add_command(const char *prefix, CommandHandler ch, int outputSize) {
		command_prefixes[num_of_commands] = prefix;
		command_handlers[num_of_commands] = ch;
		allocation_sizes[num_of_commands] = outputSize;
		++num_of_commands;
	}
	
	void send_msg(char *buf, int amt) {
	}
}
*/
