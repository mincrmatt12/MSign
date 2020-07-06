#ifndef SRV_H
#define SRV_H

#include "schedef.h"
#include <stdint.h>
#include <type_traits>
#include "protocol.h"
#include "common/bheap.h"
#include "lru.h"

namespace srv {
	// ConIO: console stuff
	struct ConIO {
		void write(const char *buf, std::size_t length);

		std::size_t remaining();
		void read(char *buf, std::size_t length);
	private:
		char buf[512]; // 0.5K buffer
		char *start = buf, *end = buf;
	};

	// Talks to the ESP8266
	//
	// Manages slots of arbitrary length data using the bheap.
	//
	// NOTE: the result of a call to data() may become invalid at any point.
	//       this may change depending on available RAM / queueing system, but presently 
	//
	// This class also handles the update procedure.
	struct Servicer final : public sched::Task, private ProtocolImpl {
		
		// non-task interface
		void init(); // starts the system, begins dma, and starts handshake procedure
		bool ready(); // is the esp talking?
		bool updating() {return is_updating;} // are we in update mode?

		using ProtocolImpl::dma_finish;

		void loop()      override;
		bool done()      override;
		bool important() override;

		// Update state introspections
		const char * update_status();

		// Data request methods
		bool set_temperature(uint16_t slotid, uint32_t temperature);

		// Data access methods
		const bheap::Block& slot(uint16_t slotid) const;
		inline const bheap::Block& operator[](uint16_t slotid) const {return slot(slotid);}
		template<typename T>
		inline const bheap::TypedBlock<T>& slot(uint16_t slotid) const {return slot(slotid).as<T>();}

		// Helpers
		bool slot_dirty(bool clear=true);

	private:
		bheap::Arena<14284> arena;
		lru::Cache<8, 4> bcache;
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
