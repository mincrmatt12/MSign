#ifndef SRV_H
#define SRV_H

#include <stdint.h>
#include <type_traits>
#include "common/slots.h"
#include "protocol.h"
#include "common/bheap.h"
#include "common/heapsize.h"

#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>

namespace srv {
	// Talks to the ESP8266
	//
	// Manages slots of arbitrary length data using the bheap.
	//
	// NOTE: the result of a call to data() may become invalid at any point, unless
	// the lock is held, in which case they will remain valid until the lock is next given.
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
		void set_temperature_group(uint32_t temperature, uint16_t len, const uint16_t * slotids);

		template<uint16_t ...Slots>
		void set_temperature_all(uint32_t temperature) {
			const static uint16_t sidlist[] = {Slots...};
			set_temperature_group(temperature, sizeof...(Slots), sidlist);
		}

		template<typename... Args>
		void set_temperature_all(uint32_t temperature, Args... args) {
			(set_temperature(args, temperature), ...);
		}

		const bheap::TypedBlock<uint8_t *>& slot(uint16_t slotid) {return slot<uint8_t *>(slotid);}
		inline const bheap::TypedBlock<uint8_t *>& operator[](uint16_t slotid) {return slot<uint8_t *>(slotid);}
		// Data access methods
		template<typename T>
		inline const bheap::TypedBlock<T>& slot(uint16_t slotid) {return _slot(slotid).as<T>();}

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

		// Read from the debug log. Will block for the timeout amount of ticks and will return up to the amount of data requested.
		// Returns the amount of data read
		size_t read_dbg(uint8_t *buf, size_t len, TickType_t timeout);

		// Write to the debug output. Will block until there is space to queue the data. Normally always flushes, if flush is set to false but
		// there is no space in the buffer we still trigger a flush to avoid deadlocking.
		//
		// Always writes the entire buffer or times out.
		bool write_dbg(uint8_t *buf, size_t len, TickType_t timeout, bool should_flush=true);

		// Request time information from the ESP
		slots::protocol::TimeStatus request_time(uint64_t& reponse, uint64_t &time_when_sent);

		// Ask ESP to refresh a specific grabber (dataset)
		void refresh_grabber(slots::protocol::GrabberID which);

		// Reset system
		[[noreturn]]
		void reset();

		// Enter/exit sleep mode
		void set_sleep_mode(bool enabled);

	private:

		const bheap::Block& _slot(uint16_t slotid);
		bheap::Arena<STM_HEAP_SIZE, lru::Cache<4, 8>> arena;

		// Called from ISR and populates queue.
		void process_command() override;

		// Wait send to complete (or return if not sending)
		void wait_for_not_sending(); // uses notification

		// Update specific routines

		// Handle a single packet in update mode
		void process_update_packet(uint8_t cmd, uint8_t len);
		// Handle an UPDATE_CMD
		void process_update_cmd(slots::protocol::UpdateCmd cmd);
		// Send a single UPDATE_STATUS
		void send_update_status(slots::protocol::UpdateStatus status);
		// Main loop in update mode.
		void do_update_logic();

		// Connect to the ESP
		void do_handshake();

		// full_cleanup is whether or not to allow doing anything that could evict packets (but still allow locking/invalidating the cache)
		void try_cleanup_heap(ssize_t space_to_clear); // returns done
		void check_connection_ping(); // verify we're still connected with last_transmission
		void send_data_requests(); // send DATA_REQUEST for all "dirty" remote blocks.

		// Are we currently doing an update? If so, we _won't_ process _any_ data requests.
		bool is_updating = false;
		uint8_t update_state = 0;

		// status shown on screen
		char update_status_buffer[16];
		// buffer of the last update package
		uint8_t update_pkg_buffer[256];
		// size of update pkg
		uint32_t update_pkg_size = 0;

		// update total size (new bin length)
		uint32_t update_total_size = 0;
		// checksum of entire update
		uint16_t update_checksum = 0;
		// how many chunks remaining in update
		uint16_t update_chunks_remaining = 0;

		// Sync primitives + log buffers + dma buffer
		SemaphoreHandle_t bheap_mutex;
		StaticSemaphore_t bheap_mutex_private;

		// Queue for incoming set temperature requests. 16 elements long (or 64 bytes)
		// The format of these requests is this struct:
		struct PendRequest {
			struct TimeRequest {
				TaskHandle_t notify;
				slots::protocol::TimeStatus &status_out;
				uint64_t &timestamp_out;
				uint64_t &start_out;
			};
			// Does _not_ sync, intended for "groups" in flash -- see set_temperature_all's template version
			struct MultiTempRequest {
				uint16_t temperature : 2;
				uint16_t amount : 14;
				const uint16_t * entries;
			};
			union {
				struct {
					uint16_t slotid : 12;
					uint16_t temperature : 2;
				};

				TimeRequest * rx_req;
				MultiTempRequest mt_req;
				slots::protocol::GrabberID refresh;
				bool sleeping;
			};
			enum PendRequestType : uint8_t {
				TypeNone = 0,
				TypeChangeTemp,
				TypeDumpLogOut,
				TypeRxTime,
				TypeChangeTempMulti,
				TypeRefreshGrabber,
				TypeSleepMode,
				TypeReset
			} type;
		};

		QueueHandle_t pending_requests;
		// Static queue allocation
		StaticQueue_t pending_requests_private;
		uint8_t       pending_requests_data[32 * sizeof(PendRequest)];

		void start_pend_request(PendRequest req);
		
		// Stream buffers for dma-ing
		//
		// Specifically only receive as [citation needed] we don't need to queue up transmissions.
		StreamBufferHandle_t dma_rx_queue;
		uint8_t              dma_rx_queue_data[2048];
		StaticStreamBuffer_t dma_rx_queue_private;

		// Stream buffers for sending/rx-ing debug stuff
		StreamBufferHandle_t log_in, log_out;
		uint8_t              log_in_data[176], log_out_data[128];
		StaticStreamBuffer_t log_in_private, log_out_private;

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
