#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include "common/slots.h"
#include "common/bheap.h"
#include "protocol.h"

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

namespace serial { 
	struct SerialInterface final : protocol::ProtocolImpl {
		void run();

		// Reset, informing the STM beforehand.
		void reset();

	private:
		void on_pkt() override;

		// ESP-local data-store
		bheap::Arena<16384> arena;

		// Request queue; very small (only 2 entries: a few size changes and at most nTasks updateRequests as they block; for you naysayers who think I should just
		// use a mailbox with a semaphore, do remember that FreeRTOS semaphores are just disguised queues- ha! who's wasting memory now, fools!)
		QueueHandle_t requests;

		// Like with the STM, we use direct to task notifications to select() on requests and packets, except here the packets are stored in an external buffer
		// so we just notify directly.

		struct UpdateParams {
			uint16_t slotid;
			uint16_t offset;
			uint16_t length;
			void * data;
			TaskHandle_t notify;
		};

		// Doesn't notify anything
		struct SizeParams {
			uint16_t slotid;
			uint16_t newsize;
		};

		struct Request {
			enum {
				TypeDataResizeRequest,
				TypeDataUpdateRequest,
				TypeReset,
				TypeEmpty
			} type = TypeEmpty;
			union {
				UpdateParams *uparams;
				SizeParams   *sparams;
			};
		} active_request;

		// Start processing a request 
		void start_request();
		// Continue processing a request (called every 500ms-ish while a request is active)
		void loop_request();
		// Called before normal packet processing if request is active, if the request handled the packet return true.
		bool packet_request();
		// Finish handling a request (check for next, etc.)
		void finish_request();

		// Process a packet normally
		void process_packet();

		// When this is true we both a) don't try to evict again and b) ignore all data sent to us and truncate it
		bool is_evicting = false;
		bool should_check_request = false;

		// Data evict loop: called when we're out of space for new data and try to send crap over to the STM
		void evict_subloop(uint16_t threshold);

		// Scan through all blocks and update/move dirty/flush blocks.
		void update_blocks();

		enum Event {
			EventPacket,
			EventQueue,
			EventAll,
			EventTimeout
		};

		Event wait_for_event(TickType_t timeout=portMAX_DELAY);
	};

	extern SerialInterface interface;
}

#endif /* ifndef SERIAL_H */
