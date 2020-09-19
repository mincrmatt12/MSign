#ifndef SRV_H
#define SRV_H

#include <stdint.h>
#include <type_traits>
#include "protocol.h"
#include "common/bheap.h"
#include "lru.h"

#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <queue.h>
#include <task.h>

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
		// Init HW & RTOS stuff.
		void init();
		bool ready(); // is the esp talking?
		bool updating() {return is_updating;} // are we in update mode?

		using ProtocolImpl::dma_finish;

		// Runs the servicer blocking.
		void run();

		// Update state introspections
		const char * update_status();

		// Data request methods
		void set_temperature(uint16_t slotid, uint32_t temperature);
		template<typename... Args>
		void set_temperature_all(uint32_t temperature, Args... args) {
			(set_temperature(args, temperature), ...);
		}

		// Data access methods
		const bheap::Block& slot(uint16_t slotid);
		inline const bheap::Block& operator[](uint16_t slotid) {return slot(slotid);}
		template<typename T>
		inline const bheap::TypedBlock<T>& slot(uint16_t slotid) {return slot(slotid).as<T>();}

		// Helpers
		bool slot_dirty(uint16_t slotid, bool clear=true);

		// Locking operations.
		//
		// You must have a lock to access slot/set_temperature.
		//
		// Once the lock is given, all references returned by slot become invalid.
		//
		// Note the lock is only on _data_; temperature updates will still occur.
		void take_lock();
		void give_lock();

	private:
		bheap::Arena<14284> arena;
		lru::Cache<8, 4> bcache;

		// Called from ISR and populates queue.

		void process_command() override;
		void process_update_cmd(uint8_t cmd);
		void do_update_logic();
		void do_handshake();

		bool do_bheap_cleanup(); // returns done
		void wait_for_not_sending(); // uses notification
		void check_connection_ping(); // verify we're still connected with last_transmission

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

		// Sync primitives + log buffers + dma buffer
		QueueHandle_t bheap_mutex;

		// Queue for incoming set temperature requests. 16 elements long (or 64 bytes)
		// The format of these requests is this struct:
		struct PendRequest {
			union {
				struct {
					uint16_t slotid : 12;
					uint16_t temperature : 2;
				};
			};
			enum PendRequestType : uint8_t {
				TypeNone = 0,
				TypeChangeTemp,
				TypePing
			} type;
		};
		QueueHandle_t pending_requests;

		void start_pend_request(PendRequest req);
		
		// Stream buffers for dma-ing
		//
		// Specifically only receive as [citation needed] we don't need to queue up transmissions.
		StreamBufferHandle_t dma_rx_queue;

		// Stream buffers for sending/rx-ing debug stuff
		StreamBufferHandle_t log_in, log_out;

		// The task to notify whenever we add something to the queue.
		//
		// This (ab)uses a fun property of stream buffers in that they use tasknotifications to work
		// and due to how the function is implemented (to deal with timeouts) if we send a bogus notification
		// ourselves whenever we update the queue we can avoid having to do weird hacks.
		TaskHandle_t this_task;
	};
	
	struct ServicerLockGuard {
		ServicerLockGuard(Servicer& srv) :
			srv(srv) {
			srv.take_lock();
		}
		~ServicerLockGuard() {
			srv.give_lock();
		}

	private:
		Servicer& srv;
	};
}


#endif
