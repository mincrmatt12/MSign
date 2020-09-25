#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include "common/slots.h"
#include "common/bheap.h"
#include "protocol.h"
#include <type_traits>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

namespace serial { 
	struct SerialInterface final : protocol::ProtocolImpl {
		void run();

		// Reset, informing the STM beforehand.
		void reset();

		// SLOT UPDATE

		// Replace a slot with the data pointed to by ptr and of length length
		void update_slot(uint16_t slotid, const void * ptr, size_t length);
		// Replace a slot with the contents of the object referenced by obj
		template<typename T>
		void update_slot(uint16_t slotid, const T& obj) {
			static_assert(!std::is_pointer<T>::value, "This will not work with pointers, use the ptr/length overload instead.");
			update_slot(slotid, &obj, sizeof(T));
		}
		// Replace a slot with a null-terminated string
		void update_slot(uint16_t slotid, const char *str) {
			update_slot(slotid, str, strlen(str));
		}
		// Clear out a slot
		void delete_slot(uint16_t slotid);

		// MANUAL SLOT UPDATE

		// Set the slot slotid's size to size, adding/truncating space if necessary.
		void allocate_slot_size(uint16_t slotid, size_t size);

		// Update part of a slot
		void update_slot_partial(uint16_t slotid, uint16_t offset, const void * ptr, size_t length);

	private:
		void on_pkt() override;

		// ESP-local data-store
		bheap::Arena<16384> arena;

		// Request queue; very small (only 2 entries: a few size changes and at most nTasks updateRequests as they block; for you naysayers who think I should just
		// use a mailbox with a semaphore, do remember that FreeRTOS semaphores are just disguised queues- ha! who's wasting memory now, fools!)
		QueueHandle_t requests = nullptr;

		// Like with the STM, we use direct to task notifications to select() on requests and packets, except here the packets are stored in an external buffer
		// so we just notify directly.

		// Update requests also will update the size if required
		struct UpdateParams {
			uint16_t slotid;
			uint16_t offset;
			uint16_t length;
			const void * data;
			TaskHandle_t notify;
		};

		// Doesn't notify anything
		//
		// Only sends a packet if the size was set to 0, otherwise just truncates the slot.
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
				SizeParams    sparams;
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

		// used for re-entrancy detection, if this is true at the beginning of update_blocks
		// we just set update_check_dirty to true and exit
		bool is_updating = false;
		// if this is true at the end of update_blocks we tail-chain another call
		bool update_check_dirty = false;

		// Scan through all blocks and update/move dirty/flush blocks.
		void update_blocks();

		enum Event {
			EventPacket,
			EventQueue,
			EventAll,
			EventTimeout
		};

		Event wait_for_event(TickType_t timeout=portMAX_DELAY);

		TaskHandle_t srv_task;
	};

	extern SerialInterface interface;
}

#endif /* ifndef SERIAL_H */
