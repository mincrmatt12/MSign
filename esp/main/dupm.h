#ifndef MSN_DUPM_H
#define MSN_DUPM_H

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <type_traits>
#include "common/bheap.h"
#include "common/heapsize.h"
#include "common/slots.h"

namespace serial {
	struct DataUpdateRequest {
		enum Type : uint8_t {
				   // sent from servicer as packets 
			TypeSetTemp,
			TypeMarkDirty,
				   // sent by grabbers
			TypeChangeSize,
			TypeGetSize,
			TypePatch,
			TypePatchWithoutMarkDirty,
			TypeTriggerUpdate,
			TypeSync,
			TypeInlinePatch,
			TypeInlinePatchWithoutMarkDirty,
		};
		union {
			struct {
				Type type;
				union {
					struct {
						uint16_t slotid;
						uint8_t newtemperature;
					} d_temp;
					struct {
						uint16_t slotid;
						uint16_t offset;
						uint16_t length;
						const void * data;
					} d_patch;
					struct {
						uint16_t slotid;
						uint16_t size;
					} d_chsize;
					struct {
						TickType_t by;
						TaskHandle_t with;
					} d_sync;
					struct {
						uint16_t slotid;
						uint16_t offset;
						uint16_t size;  
					} d_dirty;
					struct {
						uint16_t slotid;
					} d_trigger;
					struct {
						uint16_t slotid;
						size_t *cursize_out;
					} d_getsz;
				};
			};
			struct {
				Type type;
				uint8_t data[11];
				uint16_t slotid : 12;
				uint16_t length : 4;
				uint16_t offset;
			} d_inline_patch;
		};
	};

	struct DataUpdateManager {
		// Put a request into the queue.
		bool queue_request(const DataUpdateRequest &req);

		void run();

		// PACKET API

		// Returns true if a) a packet is pending in and
		// 				   b) the passed packet should be processed by us.
		//
		// This may block if the dupm is in the middle of working out which packets it needs to block on.
		bool is_packet_in_pending(const uint8_t *packet_to_check);

		// Actually process a packet, returning whether or not we actually parsed it.
		// This will clobber some bits in the task notification word.
		bool process_input_packet(uint8_t *packet);

		// Initialize queues
		void init();
	private:
		QueueHandle_t pending{};
		TaskHandle_t dupm_task, waiting_task;

		// Pending input stuff
	
		// Type-erasure for packet-pend
		struct PacketFilter {
			virtual bool wants(const uint8_t *packet_to_check) const = 0;
		};

		template<typename Func>
		struct PacketFilterWrapper : PacketFilter {
			static_assert(
				std::is_invocable_r_v<bool, Func, const uint8_t *> ||
				std::is_invocable_r_v<bool, Func, const slots::PacketWrapper<>&>
			);

			bool wants(const uint8_t *packet_to_check) const override {
				if constexpr (std::is_invocable_r_v<bool, Func, const uint8_t *>) {
					return functor(packet_to_check);
				}
				else {
					return functor(*reinterpret_cast<const slots::PacketWrapper<> *>(packet_to_check));
				}
			}

			PacketFilterWrapper(const Func& functor) : functor(functor) {}
			
			const Func& functor;
		};

		// Internal helpers
		template<typename Func>
		slots::PacketWrapper<>& wait_for_packet(Func&& filter, TickType_t timeout) {
			PacketFilterWrapper<Func> f{filter};
			pending_packet_in = &f;
			return wait_for_packet(timeout);
		}
		slots::PacketWrapper<>& wait_for_packet(TickType_t timeout);

		void finished_with_last_packet(bool processed=true);
		void hold_packets_until_wait() {
			if (critical_wait_state_active)
				return;
			critical_wait_state_active = true;
			xSemaphoreTake(critical_wait_state, portMAX_DELAY);
		}

		const PacketFilter * volatile pending_packet_in;
		uint8_t *pkt_buf;
		bheap::Arena<ESP_HEAP_SIZE> arena;
		bool sync_pending{}, critical_wait_state_active{};

		// This is a binary semaphore used to bound the state between sending a response-initiating packet and setting up the packet filter.
		SemaphoreHandle_t critical_wait_state{};

		// Handlers
		void change_size_handler(DataUpdateRequest &dur);
		void set_temp_handler(DataUpdateRequest &dur, bool send_pkt=true);
		void patch_handler(uint16_t slotid, uint16_t offset, uint16_t length, const void * data, bool mark_dirty);
		void mark_dirty_handler(DataUpdateRequest &dur);
		void trigger_update_handler(DataUpdateRequest &dur);

		// Sends all dirty blocks out
		void send_pending_dirty_blocks();

		// Allocation subroutines

		enum UseIntent {
			UseForRemoteStorage, // Make space for a remote chunk due to no local space
			UseForRemoteDisplay, // Make space for hot chunk
			TryForRemoteCache,   // Attempt to make space for warm chunk
			TryForLocalDedup     // Attempt to make space for local blocks by moving stuff to the STM (specifically warm/hot blocks to avoid duplicating them)
		};
		

		// Ensure there's a bit more space (at least enough to store 1 remote header) in our local heap. This is separate from TryForLocalDedup as that doesn't
		// try to evict cold blocks (since it's designed for trying to optimize for really low mem situations)
		void cleanout_esp_space();

		// Perform single DATA_STORE/DATA_FULFILL "cycle" (i.e. no retry handling), and return the status code (or a timeout).
		//
		// The data to send is either taken from a pointer (which you're supposed to find from the heap first), or can be null which
		// will just send zeroes.
		slots::protocol::DataStoreFulfillResult single_store_fulfill(uint16_t slotid, uint16_t offset, uint16_t length, const void * datasource, bool is_store);

		// Calculate the current "stm memory budget"
		struct StmMemoryBudgetInfo {
			// Space that nothing is using, can be allocated to anything else.
			uint16_t unused_space{};
			// Space currently used by hot blocks.
			uint16_t hot_remote{}, hot_ephemeral{};
			// Space currently used by unsupressed warm blocks
			uint16_t warm_remote{}, allocated_warm_ephemeral{};
			// Space used by all cold remote blocks (since hot/warm are already considered)
			uint16_t cold_remote{};

			// Helpers
			size_t free_by(uint16_t& parameter, size_t amount) {return xfer_into(parameter, amount, unused_space);}
			size_t use_for(uint16_t &parameter, size_t amount) {return xfer_into(unused_space, amount, parameter);}
			size_t xfer_into(uint16_t &parameter, size_t by, uint16_t& into) {
				if (parameter < by) by = parameter;
				parameter -= by;
				into += by;
				return by;
			}
			void lose_space(size_t amount) {
				if (unused_space < amount) {
					return;
				}
				unused_space -= amount;
			}

			uint16_t hot() { return hot_remote + hot_ephemeral; }
			uint16_t allocated_warm() { return warm_remote + allocated_warm_ephemeral; }
			uint16_t remote() { return cold_remote + warm_remote + hot_remote; }
		};

		// Move stuff around such that there's space available or reclaim used space from a certain slotid.
		//
		// RemoteStorage: the "extra space" portion of the budget
		// RemoteDisplay: the "hot" portion of the budget
		// RemoteCache:   try to move old stuff to give at least "delta_size_change" more bytes in the warm region, returning true if any change was made.
		bool ensure_budget_space(UseIntent reason, uint16_t slotid, ssize_t delta_size_change);
		bool ensure_budget_space(UseIntent reason, uint16_t slotid, ssize_t delta_size_change, StmMemoryBudgetInfo& use_budget);

		StmMemoryBudgetInfo calculate_memory_budget();

		// "Free" up space by searching for blocks that match "from_blocks_matching", by doing "by_doing" to them.
		// This function might free more than target_amount if there's more space in the blocks and it decides that not keeping
		// all of them is better for fragmentation reasons.
		template<typename Pred, typename Handler>
		std::enable_if_t<
			std::is_invocable_r_v<bool, Pred, const bheap::Block&> &&
			std::is_invocable_r_v<ssize_t, Handler, bheap::Block&, size_t>,
		size_t> free_space_matching_using(size_t target_amount, Pred&& from_blocks_matching, Handler&& by_doing);

		// Free entire slots (but potentially only reaping the rewards of part of them) by calling a function.
		template<typename Pred, typename Handler>
		std::enable_if_t<
			std::is_invocable_r_v<bool, Pred, const bheap::Block&> &&
			std::is_invocable_r_v<bool, Handler, uint16_t>,
		size_t> free_slots_matching_using(size_t target_amount, Pred&& from_blocks_matching, Handler&& by_doing);

		// Various free routines
		void perform_warm_ephemeral_reclaim(StmMemoryBudgetInfo& budget, size_t amount, uint16_t ignore_slotid=0xfff);
		void perform_cold_remote_reclaim(StmMemoryBudgetInfo& budget, size_t amount, uint16_t ignore_slotid=0xfff);

		// Call after freeing up space for more warm blocks on STM to send them out.
		void try_reclaim_supressed_warm();

		// Inform the STM that the temperature has been reduced
		void inform_temp_change(uint16_t slotid, uint8_t temp);

		// Move a block to the STM (useable as a handler for free_space_using)
		bool move_block_to_stm(const bheap::Block& tgt, size_t subsection_length);
	};
};

#endif
