#ifndef SRV_H
#define SRV_H

#include "sched.h"
#include <stdint.h>

namespace srv {

	// Talks to the ESP8266
	//
	// Uses up to 256 16-byte "data slots", which can be set to contain any 16-byte stream identifier.
	// Some streams are continuous, while some are acknowledge-only.
	//
	// The slots are assigned automatically, (although long term slots can be specified to optimize the searching algorithm)
	// Once a slot is no longer required to be updated, it can be stopped.
	struct Servicer : public sched::Task {
		
		// non-task interface
		void init(); // starts the system, begins dma, and starts handshake procedure
		bool ready(); // is the esp talking?

		// slot interface
		//
		// persistent means to allocate from the end of the array.:w
		//
		bool open_slot(uint16_t data_id, bool continuous, uint8_t &slot_id_out, bool persistent=false);
		inline bool slot_open(uint8_t slot_id);
		inline bool slot_connected(uint8_t slot_it);
		inline const uint8_t * slot(uint8_t slot_id);
		bool ack_slot(uint8_t slot_id);
		bool close_slot(uint8_t slot_id);

		// interrupt handlers
		void dma_finish();

		void loop()      override;
		bool done()      override;
		bool important() override;

	private:
		uint8_t slots[256][16];
		uint8_t dma_buffer[64];
		uint8_t dma_out_buffer[64];
		uint8_t slot_states[64] = {0}; // 2 bits per
		uint32_t pending_operations[6]; // pending operations, things that need to be sent out

		uint8_t state = 0;
		uint8_t pending_count = 0;
	};

}

#endif
