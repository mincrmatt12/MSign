#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include "dupm.h"
#include "protocol.h"
#include <type_traits>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

namespace serial { 
	// Must be called before init()
	bool process_stored_crashlogs();

	struct SerialInterface final : private protocol::ProtocolImpl {
		friend DataUpdateManager;

		// Initialize all queues (must be called before run())
		void init();

		// Task entry point
		void run(bool had_crash=false);

		// Reset, informing the STM beforehand.
		void reset();

		// SLOT UPDATE
		
		// Replace a slot with the data pointed to by ptr and of length length
		void update_slot_raw(uint16_t slotid, const void * ptr, size_t length, bool should_sync=true, bool should_mark_dirty=true);

		// Replace a slot with the contents of the object referenced by obj
		template<typename T>
		inline void update_slot(uint16_t slotid, const T& obj, bool should_sync=true, bool should_mark_dirty=true) {
			// Specially defer for array types
			static_assert(!std::is_pointer_v<T> || (
				std::is_array_v<T> && std::extent_v<T, 0> != 0
			), "This will not work with pointers, use update_slot_raw instead, or derefence the pointer. For bounded arrays, pass them directly or use update_slot_range");
			update_slot_raw(slotid, &obj, sizeof(T), should_sync, should_mark_dirty);
		}
		// Replace a slot with a null-terminated string
		inline void update_slot(uint16_t slotid, const char *str, bool should_sync=true, bool should_mark_dirty=true) {
			update_slot_raw(slotid, str, strlen(str) + 1, should_sync, should_mark_dirty); // include null terminator
		}
		// Replace a slot with a null-terminated string
		inline void update_slot(uint16_t slotid, char *str, bool should_sync=true, bool should_mark_dirty=true) {
			update_slot_raw(slotid, str, strlen(str) + 1, should_sync, should_mark_dirty); // include null terminator
		}
		// Replace a slot with an initializer-list of objects. No nosync version is provided since the init list
		// is almost certainly going to need syncing.
		template<typename T>
		inline void update_slot(uint16_t slotid, const std::initializer_list<T>& arr) {
			update_slot_raw(slotid, arr.begin(), arr.size() * sizeof(T));
		}

		template<typename ...Args>
		inline void update_slot_nosync(Args&&... args) {
			update_slot(std::forward<Args>(args)..., false);
		}

		template<typename ...Args>
		inline void update_slot_nodirty(Args&&... args) {
			update_slot(std::forward<Args>(args)..., true, false);
		}

		// Update slotid as if it were an array of T[], setting T[index] = obj
		template<typename T>
		inline void update_slot_at(uint16_t slotid, const T& obj, size_t index, bool should_sync=true, bool should_mark_dirty=true) {
			update_slot_partial(slotid, index * sizeof(T), &obj, sizeof(T), should_sync, should_mark_dirty);
		}

		// Update slotid as if it were an array of T[], setting T[index] = obj. sizeof(T) must be less than 11 so that the request
		// does not require synchronization. By default, the slot is not marked dirty (pass should_mark_dirty=true to change this behaviour)
		// and after updating some potentially large set of indices, trigger_slot_update should be called to push the entire change
		// to the display.
		template<typename T>
		inline void update_slot_at_inline(uint16_t slotid, const T& obj, size_t index, bool should_mark_dirty=false) {
			static_assert(sizeof(T) <= 11);
			update_slot_partial(slotid, index * sizeof(T), &obj, sizeof(T), false, should_mark_dirty);
		}

		// Update many entries in slotid, setting T[index..index+quantity] = obj[0..quantity].
		template<typename T>
		inline void update_slot_range(uint16_t slotid, const T* obj, size_t index, size_t quantity, bool should_sync=true, bool should_mark_dirty=true) {
			update_slot_partial(slotid, index * sizeof(T), obj, quantity * sizeof(T), should_sync, should_mark_dirty);
		}

		// Clear out a slot
		inline void delete_slot(uint16_t slotid) {
			allocate_slot_size(slotid, 0);
		}

		// Get the size of a slot from the arena (implicitly syncs).
		//
		// Returns 0 if not present in the arena.
		size_t current_slot_size(uint16_t slotid);

		// MANUAL SLOT UPDATE

		// Set the slot slotid's size to size, adding/truncating space if necessary.
		void allocate_slot_size(uint16_t slotid, size_t size);

		// Update part of a slot
		//
		// Note that if length is less than 11 bytes, should_sync is ignored as the request data will be inlined in the
		// queue entry.
		void update_slot_partial(uint16_t slotid, uint16_t offset, const void * ptr, size_t length, bool should_sync=true, bool should_mark_dirty=true);

		// Operation sync barrier (not per slot as requests are processed strictly in-order)
		void sync();

		// Trigger slot update (force mark dirty)
		void trigger_slot_update(uint16_t slotid);

		// SLEEP MODE

		bool is_sleeping() const {return in_sleep_mode;}
		void set_sleep_mode(bool mode);

	private:
		// The DataUpdateManager is a static variable in serial.cpp so that we can re-use it for storing crash logs.

		// Reference to the servicer task
		TaskHandle_t srv_task;

		// Handle checking for loss of comms
		void send_pings();

		// Handle a packet in the class directly, bypassing subtasks.
		void process_packet();

		// Last time we received comms with the STM (for autoreset)
		TickType_t last_comms = 0;
		bool waiting_for_ping = false, in_sleep_mode = false;
	};

	extern SerialInterface interface;
}

#endif /* ifndef SERIAL_H */
