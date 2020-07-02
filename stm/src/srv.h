#ifndef SRV_H
#define SRV_H

#include "schedef.h"
#include <stdint.h>
#include <type_traits>
#include "protocol.h"

namespace srv {
	// Implements a stupid 
	struct ConIO {
		void write(const char *buf, std::size_t length);

		std::size_t remaining();
		void read(char *buf, std::size_t length);
	private:
		char buf[512]; // 0.5K buffer
		char *start = buf, *end = buf;
	};

	// ConIO: console stuff

	// Talks to the ESP8266
	//
	// Uses up to 256 16-byte "data slots", which can be set to contain any 16-byte stream identifier.
	// Some streams are continuous, while some are acknowledge-only.
	//
	// The slots are assigned automatically, (although long term slots can be specified to optimize the searching algorithm)
	// Once a slot is no longer required to be updated, it can be stopped.
	//
	// This class also handles the update procedure.
	struct Servicer : public sched::Task, private ProtocolImpl {
		
		// non-task interface
		void init(); // starts the system, begins dma, and starts handshake procedure
		bool ready(); // is the esp talking?
		bool updating() {return is_updating;} // are we in update mode?

		// slot interface
		//
		// persistent means to allocate from the end of the array.:w
		//
		bool open_slot(uint16_t data_id, bool continuous, uint8_t &slot_id_out, bool persistent=false);
		bool slot_open(uint8_t slot_id);
		bool slot_connected(uint8_t slot_it);
		bool slot_dirty(uint8_t slot_id, bool mark_clean=false);

		const uint8_t * slot(uint8_t slot_id);
		
		template<typename T>
		inline std::enable_if_t<sizeof(T) <= 16, const T&> slot(uint8_t slot_id) const {
			return *(reinterpret_cast<const T*>(this->slots[slot_id]));
		}

		bool ack_slot(uint8_t slot_id);
		bool close_slot(uint8_t slot_id);

		using ProtocolImpl::dma_finish;

		void loop()      override;
		bool done()      override;
		bool important() override;

		// Update state introspections
		const char * update_status();

	private:
		uint8_t slots[256][16];
		uint8_t slot_states[64] = {0}; // 2 bits per
		uint8_t slot_dirties[32] = {0};
		uint32_t pending_operations[32]; // pending operations, things that need to be sent out

		uint8_t pending_count = 0;
		
		uint16_t retry_counter = 0;

		void process_command() override;
		void process_update_cmd(uint8_t cmd);
		void do_send_operation(uint32_t operation);

		// Logic related to update
		
		bool is_updating = false;
		uint8_t update_state = 0;

		char update_status_buffer[16];
		uint8_t update_pkg_buffer[256];
		uint32_t update_pkg_size;

		uint32_t update_total_size;
		uint16_t update_checksum;
		uint16_t update_chunks_remaining;
	};

}

extern srv::ConIO debug_in, debug_out;
extern srv::ConIO log_out;            


#endif
