#include "debug.h"
#include "util.h"
#include <WebSocketsServer.h>
#include "serial.h"

namespace debug {
	WebSocketsServer wss(81);
	int connected = -1;
	// 0 - unk
	// 1 - log
	// 2 - dbg
	int is_log = 0;

	int num_of_commands = 0;
	CommandHandler command_handlers[32];
	int            allocation_sizes[32];
	const char *   command_prefixes[32];

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
		wss.broadcastTXT(buf, length);
	}

	void ws_event(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
		switch (type) {
			case WStype_CONNECTED:
				{
					connected = (int)num;
					Log.println(F("connected 1"));
					return;
				}
			case WStype_DISCONNECTED:
				{
					if (is_log == 1) Log.hook = nullptr;
					is_log = 0;
					connected = -1;
					return;
				}
			case WStype_TEXT:
				{
					if (is_log == 0) {
						if (*payload == 'l') {
							is_log = 1;
							Log.hook = logger_hook;
						}
						else if (*payload == 'd') is_log = 2;
					}
					else {
						if (is_log != 2) return; 
						char * paycopy = (char *)malloc(length + 1);
						memcpy(paycopy, payload, length);
						paycopy[length] = 0;

						// Do command.
						char * value;
						int cmd = match_command((char *)paycopy, value);

						if (cmd == -1) {
							wss.sendTXT(num, "Invalid command.");
						}
						else {
							char *block = (char *)malloc(allocation_sizes[cmd]);
							block[0] = 0;
							char *block_begin = block;
							command_handlers[cmd](value, block, block + allocation_sizes[cmd]);
							if (block != block_begin) wss.sendTXT(num, block_begin, block - block_begin);
							free(block);
						}

						free(paycopy);
					}
				}
				break;
			default:
				break;
		}
	}

	void init() {
		wss.begin();
		wss.onEvent(ws_event);

		// commands
		add_command("fheap", [](const char *args, char *&begin, const char *){
			begin += sprintf_P(begin, PSTR("free heap: %d"), ESP.getFreeHeap());
		}, 32);
		add_command("quiet", [](const char *args, char *&begin, const char *){
			if (args[1] == 'e') Log.quiet_mode = true;
			if (args[1] == 'd') Log.quiet_mode = false;
		}, 8);
		add_command("stm", [](const char *args, char *&begin, const char*){
			if (args[0] == ' ') ++args;

			serial::interface.send_console_data((uint8_t *)args, strlen(args));
		}, 8);
	}

	void loop() {
		wss.loop();
	}

	void add_command(const char *prefix, CommandHandler ch, int outputSize) {
		command_prefixes[num_of_commands] = prefix;
		command_handlers[num_of_commands] = ch;
		allocation_sizes[num_of_commands] = outputSize;
		++num_of_commands;
	}
	
	void send_msg(char *buf, size_t amt) {
		if (connected != -1 && is_log == 2) {
			wss.sendTXT(connected, buf, amt);
		}
	}
}
