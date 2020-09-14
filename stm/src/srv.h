#ifndef SRV_H
#define SRV_H

#include <stdint.h>
#include <type_traits>
#include "protocol.h"
#include "common/bheap.h"
#include "lru.h"

namespace srv {
	// Talks to the ESP8266
	//
	// Manages slots of arbitrary length data using the bheap.
	//
	// NOTE: the result of a call to data() may become invalid at any point.
	//       this may change depending on available RAM / queueing system, but presently 
	//
	// This class also handles the update procedure.
	struct Servicer final : private ProtocolImpl {
		bool ready(); // is the esp talking?
		bool updating() {return is_updating;} // are we in update mode?

		using ProtocolImpl::dma_finish;

		// Runs the servicer blocking.
		void run();

		// Update state introspections
		const char * update_status();

		// Data request methods
		bool set_temperature(uint16_t slotid, uint32_t temperature);
		template<typename... Args>
		bool set_temperature_all(uint32_t temperature, Args... args) {
			return (set_temperature(temperature, args) || ...);
		}

		// Data access methods
		const bheap::Block& slot(uint16_t slotid);
		inline const bheap::Block& operator[](uint16_t slotid) {return slot(slotid);}
		template<typename T>
		inline const bheap::TypedBlock<T>& slot(uint16_t slotid) {return slot(slotid).as<T>();}

		// Helpers
		bool slot_dirty(uint16_t slotid, bool clear=true);

	private:
		// Init HW
		void init();

		bheap::Arena<14284> arena;
		lru::Cache<8, 4> bcache;

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
		uint16_t should_reclaim_amt = 0;
	};

}

#endif
