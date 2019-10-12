#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <Arduino.h>

namespace serial { 

	enum Command : uint8_t {
		HANDSHAKE_INIT = 0x10,
		HANDSHAKE_RESP,
		HANDSHAKE_OK,
		OPEN_CONN = 0x20,
		CLOSE_CONN,
		NEW_DATA,
		ACK_OPEN_CONN = 0x30,
		ACK_CLOSE_CONN,
		SLOT_DATA = 0x40,
		RESET = 0x50,
		PING,
		PONG,
		CONSOLE_MSG = 0x70
	};

	// returns true if new data was added to the buffer and length
	typedef bool (*QueryHandler)(uint16_t data_id, uint8_t * buffer, uint8_t & length);
	// void, called whenever a slot is opened
	typedef void (*OpenHandler) (uint16_t data_id);

	// returns ~0 on fail.
	uint16_t search_for(uint16_t val, uint16_t array[64]);

	// debug command
	void slotstats(const char * args, char *&buffer, const char *end);

	struct SerialInterface {
		bool ensure_handshake();
		void loop();
		void register_debug_commands();

		// Sends data to any slot with this data id, immediately -- can block.
		void update_data(uint16_t data_id, const uint8_t * buffer, uint8_t length);

		// Register a polled data handler
		void register_handler(const QueryHandler handler);
		void register_handler(const OpenHandler handler);

		// Reset, informing the STM beforehand.
		void reset();

		// Send data to the STM debug console port
		void send_console_data(const uint8_t * buffer, size_t length);

	private:
		void handle_command(serial::Command cmd, uint8_t size, uint8_t *buf);
		void send_data_to(uint8_t slot_id, const uint8_t * buffer, uint8_t length);
		void update_polled_data(uint8_t slot_id);
		void update_open_handlers(uint8_t data_id);

		friend void ::serial::slotstats(const char * args, char *&buffer, const char *end);

		uint16_t slots_continuous[256];
		uint16_t slots_polled[256];

		QueryHandler  handlers[8];
		OpenHandler o_handlers[8];

		uint8_t waiting_size = 0;
		uint8_t pending_command = 0;
		uint8_t number_of_handlers = 0;
		uint8_t number_of_o_handlers = 0;

		bool init_state = false;
	};

	extern SerialInterface interface;
}

#endif /* ifndef SERIAL_H */
