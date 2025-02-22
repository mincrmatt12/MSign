#ifndef SRV_H
#define SRV_H

#include <stdint.h>
#include "common/slots.h"
#include "protocol.h"
#include "common/bheap.h"
#include "common/heapsize.h"

#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <semphr.h>
#include <queue.h>
#include <task.h>

namespace crash::srvd {
	struct ServicerDebugAccessor;
}

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
		// For debugging
		friend struct ::crash::srvd::ServicerDebugAccessor;

		// Init HW & RTOS stuff.
		void init();
		bool ready(); // is the esp talking?
		bool updating() const {return handshake_state == EspUpdating;} // are we in update mode?
		bool crashed() const  {return handshake_state == EspCrashed;}  // did the esp crash?

		using ProtocolImpl::dma_finish;

		// Runs the servicer blocking.
		void run();

		// Update state introspections
		const char * update_status();
		// Changes every time the update_status changes -- screen task uses this to yield so that status is shown before disabling flash
		uint32_t     update_status_version();

		// Data request methods
		void set_temperature(uint16_t slotid, uint32_t temperature);
		void set_temperature_group(uint32_t temperature, uint16_t len, const uint16_t * slotids, bool sync=false);

		template<uint16_t ...Slots>
		void set_temperature_all(uint32_t temperature) {
			const static uint16_t sidlist[] = {Slots...};
			set_temperature_group(temperature, sizeof...(Slots), sidlist);
		}

		template<typename... Args>
		void set_temperature_all(uint32_t temperature, Args... args) {
			(set_temperature(args, temperature), ...);
			immediately_process();
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

		// Get the current ADC calibration (or return an error code)
		slots::protocol::AdcCalibrationResult get_adc_calibration(slots::protocol::AdcCalibration& calibration_out);

		// Set the current ADC calibration. Returns false if the request timed out.
		bool set_adc_calibration(const slots::protocol::AdcCalibration& calibration);

		// Reset system
		[[noreturn]]
		void reset();

		// Enter/exit sleep mode
		void set_sleep_mode(bool enabled);

		// Immediately request that the servicer respond to all pending requests, instead of every 400ms
		void immediately_process();

		// Are we currently sleeping?
		bool is_sleeping() { return is_in_sleep_mode; }

		// Debug information; returned by debug_information().
		// Contains various properties about the servicer's state
		struct DebugInfo {
			// QUEUE/COMM STATS
			UBaseType_t pending_requests_count = 0; // number of requests pending in internal queue
			uint64_t ticks_since_last_communication = 0; // time since last comms with esp
			bool ping_is_in_flight; // is there a ping in flight due to no communication
			// ARENA STATS
			size_t free_space_arena = 0;
			size_t free_space_cleanup_arena = 0;
			size_t used_hot_space_arena = 0;
		};

		DebugInfo get_debug_information();

		// Pending request queue entries -- made public for debug reasons.
		struct PendRequest {
			struct TimeRequest {
				TaskHandle_t notify;
				slots::protocol::TimeStatus &status_out;
				uint64_t &timestamp_out;
				uint64_t &start_out;
				// used to avoid writing into weird memory
				volatile int magic = 0x1234abcd;

				~TimeRequest() {magic = 0;}
				operator bool() const {return magic == 0x1234abcd;}
			};
			struct CalibrationRequest {
				TaskHandle_t notify;
				slots::protocol::AdcCalibration& update_or_read_calibration;
				// used to avoid writing into weird memory
				volatile int magic = 0xca1b4a4e;
				slots::protocol::AdcCalibrationResult result_write;

				~CalibrationRequest() {magic = 0;}
				operator bool() const {return magic == 0xca1b4a4e;}
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
				CalibrationRequest * calibrate;
				MultiTempRequest mt_req;
				slots::protocol::GrabberID refresh;
				bool sleeping;
				TaskHandle_t sync_with;
			};
			enum PendRequestType : uint8_t {
				TypeNone = 0,
				TypeChangeTemp,
				TypeDumpLogOut,
				TypeRxTime,
				TypeChangeTempMulti,
				TypeRefreshGrabber,
				TypeSleepMode,
				TypeReset,
				TypeSync,
				TypeGetCalibration,
				TypeSetCalibration,
			} type;
		};
	private:
		const bheap::Block& _slot(uint16_t slotid);
		static bheap::Arena<STM_HEAP_SIZE, lru::Cache<16, 4>> arena;

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

		// Flash routines
		void begin_update();
		void update_append_data(bool already_erased=false);
		void update_append_data_copy();
		void erase_flash_sector();
		void wait_for_update_status_onscreen();

		// Connect to the ESP
		void do_handshake();

		// full_cleanup is whether or not to allow doing anything that could evict packets (but still allow locking/invalidating the cache)
		void try_cleanup_heap(ptrdiff_t space_to_clear); // returns done
		void check_connection_ping(); // verify we're still connected with last_transmission
		void send_data_requests(); // send DATA_REQUEST for all "dirty" remote blocks.

		// Are we currently doing an update? If so, we _won't_ process _any_ data requests.
		volatile enum : uint8_t {
			EspNormal,
			EspUpdating,
			EspCrashed
		} handshake_state = EspNormal;
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
		// used for syncing display state
		uint32_t update_status_cookie = 0;

		// keeps track of where in flash we are
		uint32_t update_tempflash_data_ptr;
		// number of bytes written to current sector
		uint32_t update_tempflash_data_counter = 0;
		// which flash sector
		uint8_t update_tempflash_sector_counter = 5;

		// Sync primitives + log buffers + dma buffer
		SemaphoreHandle_t bheap_mutex;
		StaticSemaphore_t bheap_mutex_private;

		// Queue for incoming set temperature requests. 16 elements long (or 64 bytes)

		QueueHandle_t pending_requests;
		// Static queue allocation
		StaticQueue_t pending_requests_private;
		uint8_t       pending_requests_data[40 * sizeof(PendRequest)];

		// Returns if no continuation is requred
		bool start_pend_request(PendRequest &req);
		bool needs_temperature_update(uint16_t slotid, uint16_t temperature);
		
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
		//
		// We also store a bool that is only true when waiting in the main xStreamBufferReceive to avoid interrupting ones
		// that are designed not to be interrupted.
		TaskHandle_t this_task;

		volatile bool can_interrupt_with_notification = false;

		// Are we currently in sleep mode?
		volatile bool is_in_sleep_mode = false;
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
