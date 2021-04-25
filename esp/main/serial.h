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
	struct SerialSubtask;

	struct SerialInterface final : protocol::ProtocolImpl {
		friend struct SerialSubtask;

		void run();

		// Reset, informing the STM beforehand.
		void reset();

		// SLOT UPDATE

		// Replace a slot with the data pointed to by ptr and of length length
		void update_slot(uint16_t slotid, const void * ptr, size_t length);
		// Replace a slot with the contents of the object referenced by obj
		template<typename T>
		inline void update_slot(uint16_t slotid, const T& obj) {
			static_assert(!std::is_pointer<T>::value, "This will not work with pointers, use the ptr/length overload instead.");
			update_slot(slotid, &obj, sizeof(T));
		}
		// Replace a slot with a null-terminated string
		inline void update_slot(uint16_t slotid, const char *str) {
			update_slot(slotid, str, strlen(str) + 1); // include null terminator
		}
		// Clear out a slot
		inline void delete_slot(uint16_t slotid) {
			allocate_slot_size(slotid, 0);
		}

		// MANUAL SLOT UPDATE

		// Set the slot slotid's size to size, adding/truncating space if necessary.
		void allocate_slot_size(uint16_t slotid, size_t size);

		// Update part of a slot
		void update_slot_partial(uint16_t slotid, uint16_t offset, const void * ptr, size_t length);

	private:
		void on_pkt() override;

		// ESP-local data-store
		bheap::Arena<12*1024> arena;

		// Event queue, containing ServicerEvent records:
		//   - either packets, or task requests
		//   - should be around 20 bytes each
		QueueHandle_t events = nullptr;

		// Overflow request queue, used when no tasks are available to handle a request. Larger than the event queue since
		// unlike the event queue this one shouldn't be being cleared constantly.
		QueueHandle_t request_overflow = nullptr;

		// Like with the STM, we use direct to task notifications to select() on requests and packets, except here the packets are stored in an external buffer
		// so we just notify directly.

		struct SerialEvent {
			const inline static uint32_t EventTypePacket = 0;
			const inline static uint32_t EventTypeRequest = 1;

			uint32_t event_type : 1;

			const inline static uint32_t EventSubtypeSizeUpdate = 0;
			const inline static uint32_t EventSubtypeDataUpdate = 1;
			const inline static uint32_t EventSubtypeSync = 2;
			const inline static uint32_t EventSubtypeSystemReset = 3;

			uint32_t event_subtype : 3;

			const inline static uint32_t TransferFlagFreeBuffer = 1; // these are currently unimplemented, but may provide an efficient alternative
			const inline static uint32_t TransferFlagCopyBuffer = 2; // to explicit syncing

			uint32_t transfer_flags : 4;

			// maybe additional flags?

			// specific request info
			union {
				struct {
					uint16_t slotid;
					uint16_t newsize;
				} size_update;
				struct {
					TaskHandle_t tonotify;
					uint16_t slotid;
				} sync;
				struct {
					uint16_t slotid;
					uint16_t offset;
					const void * data;
					uint16_t length;
				} data_update;
				struct {
					const char * resetreason; // null for normal reset, string for unexpected error reset.
				} system_reset;
			} d;
		};

		// The servicer uses _subtasks_, which are like little async state machine things for handling packet sequences efficiently.
		SerialSubtask *subtasks[8] {};

		// Update all task timeouts
		void update_task_timeouts();

		// Handle finishing a subtask
		void finish_subtask(int pos);

		// Handle a specific request (not a packet)
		//
		// Return false if the request cannot be processed right now and should be placed in the overflow queue.
		bool handle_request(SerialEvent &evt);

		TaskHandle_t srv_task;

		// Handle checking for loss of comms
		void send_pings();

		// Handle a packet in the class directly, bypassing subtasks.
		void process_packet_directly();

		// Last time we received comms with the STM (for autoreset)
		TickType_t last_comms = 0;
		bool waiting_for_ping = false, arena_dirty = false;

		// Try to start a task, returning its id if succesful or -1 if not enough space.
		template<typename TaskType, typename ...Args>
		inline int try_start_task(Args&& ...args);

		// Mark arena dirty and re-update
		void mark_arena_dirty();
	};

	extern SerialInterface interface;

	// A single subtask for the servicer
	struct SerialSubtask {
		virtual ~SerialSubtask() = default;
		// Callback interface

		// Called whenever a different task finishes. This runs immediately after
		// the task is removed from the task array.
		virtual void on_task_end(const SerialSubtask& other) {};
		// Called whenever a packet is receieved
		virtual bool on_packet  (uint8_t * packet) {return false;};
		// Called whenever the per-subtask timeout expires.
		virtual void on_timeout () {timeout = 0;};
		// Called at the beginning of the task.
		virtual void start() = 0;

		// Get the current timeout time. Returns 0 if no timeout is set. If the
		// current time is greater than this value, on_timeout() should be called.
		const TickType_t& current_timeout() const {return timeout;}

		// Relevant slotid for this task. Set to SlotEmpty if this task is not dealing with a slot.
		uint16_t slotid = bheap::Block::SlotEmpty;

		// Get what kind of subtask this is (useful for on_task_end-based scheduling)
		virtual int subtask_type() const = 0;
		
		virtual bool should_block_updates_for(uint16_t slotid) { return false; }
	protected:

		// The current timeout. This should be set to 0 when not in use (and especially when returning from on_timeout())
		TickType_t timeout = 0;

		// Complete this task
		void finish();

		// Determine this task's ID
		int get_task_id() const;

		// Send a packet
		inline void send_packet(const void* packet) { interface.send_pkt(packet); }

		// Get the array of all tasks. Avoids making all subtasks friends of SerialInterface.
		inline SerialSubtask ** get_tasks() { return &interface.subtasks[0]; }

		template<typename Predicate>
		SerialSubtask * other_tasks_match(Predicate&& predicate) {
			for (int i = 0; i < 8; ++i) {
				if (interface.subtasks[i] == this || !interface.subtasks[i]) continue;
				if (predicate(*interface.subtasks[i])) return interface.subtasks[i];
			}
			return nullptr;
		}

		// Get the bheap arena
		inline auto& get_arena() { return interface.arena; }

		// Try to start a task, returning its id if succesful or -1 if not enough space.
		template<typename TaskType, typename ...Args>
		inline int try_start_task(Args&& ...args) {
			return interface.try_start_task<TaskType>(std::forward<Args>(args)...);
		}

		// Mark arena dirty
		inline void mark_arena_dirty() { interface.mark_arena_dirty(); }
	};

}

#endif /* ifndef SERIAL_H */
