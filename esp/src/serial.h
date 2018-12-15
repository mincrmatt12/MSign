#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

namespace serial { 

	// returns true if new data was added to the buffer and length
	typedef bool (*QueryHandler)(uint16_t slot_id, uint8_t *& buffer, uint8_t & length);

	struct SerialInterface {
		void loop();
		void update_data(uint16_t slot_id, uint8_t * buffer, uint8_t length);
		void register_handler(const QueryHandler handler);

	private:
		uint16_t slots_continuous[256];
		uint16_t slots_polled[256];

		QueryHandler handlers[8];
	};

	SerialInterface interface;

}

#endif /* ifndef SERIAL_H */
