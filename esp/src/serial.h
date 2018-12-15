#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

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
		SLOT_DATA = 0x40
	};

	// returns true if new data was added to the buffer and length
	typedef bool (*QueryHandler)(uint16_t slot_id, uint8_t *& buffer, uint8_t & length);

	struct SerialInterface {
		void ensure_handshake();
		void loop();
		void update_data(uint16_t slot_id, uint8_t * buffer, uint8_t length);
		void register_handler(const QueryHandler handler);

	private:
		uint16_t slots_continuous[256];
		uint16_t slots_polled[256];

		QueryHandler handlers[8];
	};

	extern SerialInterface interface;
}

#endif /* ifndef SERIAL_H */
