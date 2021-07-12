#include "dupm.h"
#include "projdefs.h"
#include "serial.h"

#include <algorithm>
#include <esp_log.h>
#include <utility>
#include <numeric>

static const char * TAG = "dupm";

namespace serial {

	// Constants for memory allocation: target free space at all times
	const inline static size_t target_free_space_buffer = 64;
	const inline static size_t maximum_sliver_for_free = 8;

	const inline static uint32_t TaskBitPacketRx = 1 << 0;
	const inline static uint32_t TaskBitPacketOK = 1 << 14;
	const inline static uint32_t TaskBitPacketNOK = 1 << 15;

	void DataUpdateManager::init() {
		pending = xQueueCreate(10, sizeof(DataUpdateRequest));
		if (!pending) {
			ESP_LOGE(TAG, "failed to create dur queue");
			while (1) {;}
		}
	}

	bool DataUpdateManager::queue_request(const DataUpdateRequest& req) {
		return xQueueSend(pending, &req, pdMS_TO_TICKS(2000)) == pdPASS;
	}

	bool DataUpdateManager::is_packet_in_pending(const uint8_t *check_packet) {
		if (!pending_packet_in) return false;
		if (!pending_packet_in->wants(check_packet)) return false;
		return true;
	}

	bool DataUpdateManager::process_input_packet(uint8_t *packet) {
		waiting_task = xTaskGetCurrentTaskHandle();
		pkt_buf = packet;
		// Send us the packet address (via the notification value)
		xTaskNotify(dupm_task, TaskBitPacketRx, eSetBits);
		uint32_t v;
wrong_bits:
		if (!xTaskNotifyWait(0, TaskBitPacketOK | TaskBitPacketNOK, &v, portMAX_DELAY)) {
			waiting_task = 0;
			return false;
		}
		if (v & TaskBitPacketOK) {
			waiting_task = 0;
			return true;
		}
		if (v & TaskBitPacketNOK) {
			waiting_task = 0;
			return false;
		}
		goto wrong_bits;
	}

	slots::PacketWrapper<>& DataUpdateManager::wait_for_packet(TickType_t timeout) {
		static slots::PacketWrapper<> nullp;
		uint32_t v;
		if (!xTaskNotifyWait(0, 0xffff'ffff, &v, timeout)) {
			pending_packet_in = nullptr;
			memset(&nullp, 0, sizeof(nullp));
			return nullp;
		}
		pending_packet_in = nullptr;
		return *(slots::PacketWrapper<> *)pkt_buf;
	}

	void DataUpdateManager::finished_with_last_packet(bool v) {
		if (!waiting_task) return;
		xTaskNotify(waiting_task, v ? TaskBitPacketOK : TaskBitPacketNOK, eSetBits);
		waiting_task = 0;
	}

	void DataUpdateManager::send_pending_dirty_blocks() {
		// Ship out all blocks that are >= Warm with the dirty flag set.
		for (auto& b : arena) {
			if (!(b.location == bheap::Block::LocationCanonical && b.temperature >= bheap::Block::TemperatureWarm && b.flags & bheap::Block::FlagDirty)) continue;
			for (int tries = 0; tries < 4; ++tries) {
				switch (single_store_fulfill(b.slotid, arena.block_offset(b), b.datasize, b.data(), false)) {
					case slots::protocol::DataStoreFulfillResult::Ok:
						goto block_ok;
					case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_Failed:
						// not enough space for block, if it's warm set it to non-warm
						if (b.temperature == bheap::Block::TemperatureWarm) {
							ESP_LOGW(TAG, "out of space shipping out warm block, deferring slot...");
							inform_temp_change(b.slotid, bheap::Block::TemperatureCold);
							arena.set_temperature(b.slotid, bheap::Block::TemperatureCold); // not the "wants warm" variant since this shouldn't happen normally.
						}
						[[fallthrough]];
					case slots::protocol::DataStoreFulfillResult::IllegalState:
					case slots::protocol::DataStoreFulfillResult::InvalidOrNak:
						ESP_LOGE(TAG, "error sending block.");
						goto block_fail;
					case slots::protocol::DataStoreFulfillResult::Timeout:
					case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_TryAgain:
						vTaskDelay(40);
						continue;
				}
			}
block_fail:
			ESP_LOGE(TAG, "Failed to ship out block (%p) (%03x)", &b, b.slotid);
			continue;
block_ok:
			// mark block non-dirty
			b.flags &= ~bheap::Block::FlagDirty;
		}
		sync_pending = false;
	}

	void DataUpdateManager::run() {
		dupm_task = xTaskGetCurrentTaskHandle();

		while (true) {
			// Wait for a request
			DataUpdateRequest dur;
			
			if (xQueueReceive(pending, &dur, sync_pending ? pdMS_TO_TICKS(50) : portMAX_DELAY) == pdFALSE) {
				if (sync_pending) send_pending_dirty_blocks();
				continue;
			}

			// Process it
			switch (dur.type) {
				case DataUpdateRequest::TypeSync:
					// Notify the calling task if the current time is less than the timeout
					if (xTaskGetTickCount() < dur.d_sync.by)
						xTaskNotify(dur.d_sync.with, 0xffff'ffff, eSetValueWithOverwrite);
					// Send out blocks marked for sending
					send_pending_dirty_blocks();
					break;
				case DataUpdateRequest::TypeChangeSize:
					// Process change size
					change_size_handler(dur);
					break;
				case DataUpdateRequest::TypeSetTemp:
					// Change temperature
					set_temp_handler(dur);
					break;
				case DataUpdateRequest::TypePatch:
					// Update data
					patch_handler(dur);
					break;
				case DataUpdateRequest::TypeMarkDirty:
					// Mark region dirty (or just send out blocks, depending on temp)
					mark_dirty_handler(dur);
					break;
				default:
					break;
			}
		}
	}

	void DataUpdateManager::patch_handler(DataUpdateRequest &dur) {
		if (
		// Ensure slot exists
			!arena.contains(dur.d_patch.slotid) ||
		// Ensure slot is correct size
		    (dur.d_patch.offset + dur.d_patch.length > arena.contents_size(dur.d_patch.slotid))
		) {
			ESP_LOGE(TAG, "dropping update request for slot %03x which is wrong size/doesn't exist.", dur.d_patch.slotid);
			return;
		}
		
		// Update contents, storing directly.
		if (!arena.update_contents(dur.d_patch.slotid, dur.d_patch.offset, dur.d_patch.length, dur.d_patch.data, [&](uint32_t offset, uint32_t length, const void *data){
			for (int tries = 0; tries < 4; ++tries) {
				switch (single_store_fulfill(dur.d_patch.slotid, offset, length, data, true)) {
					case slots::protocol::DataStoreFulfillResult::Ok:
						return true;
					case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_Failed:
					case slots::protocol::DataStoreFulfillResult::IllegalState:
					case slots::protocol::DataStoreFulfillResult::InvalidOrNak:
						ESP_LOGE(TAG, "block got invalid state on stm for patch remote.");
						return false;
					case slots::protocol::DataStoreFulfillResult::Timeout:
					case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_TryAgain:
						vTaskDelay(40);
						continue;
				}
			}
			ESP_LOGW(TAG, "timed out sending remote block, dropping.");
			return true;
		})) {
			ESP_LOGE(TAG, "unexpected failure to update blocks.");
			return;
		}

		sync_pending = true;
	}

	void DataUpdateManager::mark_dirty_handler(DataUpdateRequest &dur) {
		for (auto *b = &arena.get(dur.d_dirty.slotid, dur.d_dirty.offset); b && arena.block_offset(*b) < dur.d_dirty.offset + dur.d_dirty.size; b = b->next()) {
			// Is this a canonical block? If so, we mark it dirty as long as it's not queued to be flushed
			if (b->location == bheap::Block::LocationCanonical) {
				b->flags |= bheap::Block::FlagDirty;
			}
			else if (b->location == bheap::Block::LocationRemote) {
				ESP_LOGW(TAG, "Block %p has not yet been moved back to the esp, so we're ignoring the request to clear it", b);
			}
			else {
				ESP_LOGW(TAG, "Skipping %p", b);
			}
		}
	}

	slots::protocol::DataStoreFulfillResult DataUpdateManager::single_store_fulfill(uint16_t slotid, uint16_t offset, uint16_t length, const void * datasource, bool is_store) {
		{
			slots::PacketWrapper<255> pkt;
			pkt.init(is_store ? slots::protocol::DATA_STORE : slots::protocol::DATA_FULFILL);

			pkt.put(length, 4);
			for (uint16_t suboff = 0; suboff < length; suboff += (255-6)) {
				bool start = suboff == 0;
				bool end   = suboff + (255-6) >= length;
				if (end) {
					pkt.size = 6 + length - suboff;
				}
				pkt.put<uint16_t>((start << 15) | (end << 14) | (slotid & 0xfff), 0);
				pkt.put<uint16_t>(suboff + offset, 2);
				if (datasource) memcpy(pkt.data() + 6, &((const uint8_t *)datasource)[suboff], pkt.size - 6);

				serial::interface.send_pkt(pkt);
			}
		}

		auto &v = wait_for_packet([&](const slots::PacketWrapper<>& p){
			return p.from_stm() && p.cmd() == (is_store ? slots::protocol::ACK_DATA_STORE : slots::protocol::ACK_DATA_FULFILL) && p.at<uint16_t>(0) == slotid && p.at<uint16_t>(2) == offset && p.at<uint16_t>(4) == length;
		}, pdMS_TO_TICKS(500));

		if (!v) {
			ESP_LOGD(TAG, "timed out in ssf");
			return slots::protocol::DataStoreFulfillResult::Timeout;
		}

		else {
			auto code = (slots::protocol::DataStoreFulfillResult)v.data()[6];
			finished_with_last_packet();
			return code;
		}
	}

	void DataUpdateManager::set_temp_handler(DataUpdateRequest& dur, bool send_pkt) {
		// Prepare an ack packet
		slots::PacketWrapper<3> pkt_ack;
		pkt_ack.init(slots::protocol::ACK_DATA_TEMP);
		pkt_ack.put(dur.d_temp.slotid, 0);
		pkt_ack.data()[2] = dur.d_temp.newtemperature;

		// If the data is not present, ignore this request and NAK
		if (!arena.contains(dur.d_temp.slotid)) {
			if (!arena.add_block(dur.d_temp.slotid, bheap::Block::LocationCanonical, 0)) {
				cleanout_esp_space();
				if (!arena.add_block(dur.d_temp.slotid, bheap::Block::LocationCanonical, 0)) {
send_nak:
					// Respond with NAK
					pkt_ack.data()[2] = 0xff;
					if (!send_pkt) return;
					serial::interface.send_pkt(pkt_ack);
					return;
				}
			}
		}
		auto current_temp = arena.get(dur.d_temp.slotid).temperature;
		// If the current temperature is equal, ignore.
		if (current_temp == dur.d_temp.newtemperature) {
			if (!send_pkt) return;
			serial::interface.send_pkt(pkt_ack);
			return;
		}

		switch (dur.d_temp.newtemperature) {
			case bheap::Block::TemperatureHot:
				{
					// Ensure there's enough space in the budget if we're coming from cold
					if (current_temp < bheap::Block::TemperatureWarm) {
						ssize_t delta = arena.contents_size(dur.d_temp.slotid);
						// Compute initial budget
						auto budget = calculate_memory_budget();
						// Allocate space from remote blocks, if any
						std::for_each(arena.begin(dur.d_temp.slotid), arena.end(dur.d_temp.slotid), [&](bheap::Block& b){
							if (b.location != bheap::Block::LocationRemote) return;
							// Allocate this block's space
							delta -= b.datasize;
							budget.xfer_into(budget.cold_remote, b.datasize, budget.hot_remote);
						});
						// Do we need to do further allocation?
						if (budget.unused_space < delta) {
							// Ensure there's enough space
							if (!ensure_budget_space(UseForRemoteDisplay, dur.d_temp.slotid, delta, budget)) {
								ESP_LOGE(TAG, "out of space trying to allocate hot for %03x; please adjust heap sizes", dur.d_temp.slotid);
								// send nak
								goto send_nak;
							}
						}

						sync_pending = true;
					}

					// Send the ack before sending remote blocks
					if (send_pkt) serial::interface.send_pkt(pkt_ack);

warm_hot_send_remotes:
					// Ensure the entire block is either remote or non-remote
					if (std::any_of(arena.begin(dur.d_temp.slotid), arena.end(dur.d_temp.slotid), [](auto& b){return b.location == bheap::Block::LocationRemote;})) {
						// For all remote blocks, ship them out to the STM. We also perform this for warm, so only in the low-space condition does this happen _now_.
						std::for_each(arena.begin(dur.d_temp.slotid), arena.end(dur.d_temp.slotid), [&](bheap::Block& b){
							if (b.location == bheap::Block::LocationRemote) return;
							// Ship out block
							for (int tries = 0; tries < 3; ++tries) {
								switch (single_store_fulfill(b.slotid, arena.block_offset(b), b.datasize, b.data(), true)) {
									case slots::protocol::DataStoreFulfillResult::Ok:
										// finish by setting block to remote
										arena.set_location(b.slotid, arena.block_offset(b), b.datasize, bheap::Block::LocationRemote);
										return;
									case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_Failed:
									case slots::protocol::DataStoreFulfillResult::IllegalState:
									case slots::protocol::DataStoreFulfillResult::InvalidOrNak:
										ESP_LOGE(TAG, "bad state for block");
										return;
									case slots::protocol::DataStoreFulfillResult::Timeout:
									case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_TryAgain:
										vTaskDelay(80);
										continue;
								}
							}
							ESP_LOGE(TAG, "Failed to ship out remote block in Warm/Hot (%p)", &b);
						});
					}

					// Finish setting temperature
					arena.set_temperature(dur.d_temp.slotid, dur.d_temp.newtemperature);
				}
				return;
			case bheap::Block::TemperatureWarm:
				{
					// We only need to adjust for more space if coming from cold (similarly to above)
					if (current_temp != bheap::Block::TemperatureHot) {
						ssize_t delta = arena.contents_size(dur.d_temp.slotid);
						// Compute initial budget
						auto budget = calculate_memory_budget();
						// Allocate space from remote blocks, if any
						std::for_each(arena.begin(dur.d_temp.slotid), arena.end(dur.d_temp.slotid), [&](bheap::Block& b){
							if (b.location != bheap::Block::LocationRemote) return;
							// Allocate this block's space
							delta -= b.datasize;
							budget.xfer_into(budget.cold_remote, b.datasize, budget.warm_remote);
						});
						// Do we need to do further allocation?
						if (budget.unused_space < delta) {
							// Ensure there's enough space
							if (!ensure_budget_space(TryForRemoteCache, dur.d_temp.slotid, delta, budget)) {
								ESP_LOGW(TAG, "out of space trying to allocate warm for %03x; deferring", dur.d_temp.slotid);
								arena.set_temperature(dur.d_temp.slotid, bheap::Block::TemperatureColdWantsWarm);
								goto send_nak;
							}
						}
						
						// Queue up flush
						sync_pending = true;
					}

					if (send_pkt) serial::interface.send_pkt(pkt_ack);
				}

				goto warm_hot_send_remotes;
			case bheap::Block::TemperatureCold:
				{
					// Just set the temperature and let the reclaimer figure it out
					arena.set_temperature(dur.d_temp.slotid, bheap::Block::TemperatureCold);
					if (send_pkt) serial::interface.send_pkt(pkt_ack);
					try_reclaim_supressed_warm();
					return;
				}
			default:
				ESP_LOGW(TAG, "invalid temperature set of %03x to %d", dur.d_temp.slotid, dur.d_temp.newtemperature);
				goto send_nak;
		}

	}

	template<typename Pred, typename Handler>
	std::enable_if_t<
		std::is_invocable_r_v<bool, Pred, const bheap::Block&> &&
		std::is_invocable_r_v<ssize_t, Handler, const bheap::Block&, size_t>,
	size_t> DataUpdateManager::free_space_matching_using(size_t target_amount, Pred&& from_blocks_matching, Handler&& by_doing) {
		if constexpr (std::is_same_v<std::invoke_result_t<Handler, const bheap::Block&, size_t>, bool>) {
			// call with wrapper that assumes entire block is sent or not
			return free_space_matching_using(target_amount, std::forward<Pred>(from_blocks_matching), [sub=std::forward<Handler>(by_doing)](const bheap::Block& b, size_t a){
				return sub(b, a) ? a : -1;
			});
		}
		else {
			size_t amount = 0;
			while (amount < target_amount) {
				bheap::Block *selected = nullptr;
				for (auto& block : arena) {
					if (!std::forward<Pred>(from_blocks_matching)(block)) continue;
					if (!selected || (selected->datasize <= (target_amount - amount) + maximum_sliver_for_free && selected->datasize < block.datasize)) selected = &block;
				}

				// If there are no more blocks to free, return the amount we freed anyways
				if (!selected) return amount;

				size_t use_amount = (target_amount - amount);

				// Otherwise free:
				if (selected->datasize <= (target_amount - amount) + maximum_sliver_for_free) {
					use_amount = selected->datasize;
				}

				if (auto amt = std::forward<Handler>(by_doing)(*selected, use_amount); amt != -1) amount += amt;
				else {
					ESP_LOGW(TAG, "free_space_matching handler returned -1, stopping early.");
					return amount;
				}
			}

			return amount;
		}
	}

	template<typename Pred, typename Handler>
	std::enable_if_t<
		std::is_invocable_r_v<bool, Pred, const bheap::Block&> &&
		std::is_invocable_r_v<bool, Handler, uint16_t>,
	size_t> DataUpdateManager::free_slots_matching_using(size_t target_amount, Pred&& from_blocks_matching, Handler&& by_doing) {
		size_t amount = 0;

		while (amount < target_amount) {
			uint16_t selected = bheap::Block::SlotEmpty;
			size_t selected_reclaim = 0;

			for (auto& block : arena) {
				if (!block || &block != &arena.get(block.slotid)) continue;
				size_t this_reclaim = std::accumulate(arena.begin(block.slotid), arena.end(block.slotid), (size_t)0, [pred = std::forward<Pred>(from_blocks_matching)](auto& a, auto& b){
					return pred(b) ? a + b.datasize : a;
				});

				if (this_reclaim == 0) continue;
				if (selected == bheap::Block::SlotEmpty || (selected_reclaim <= (target_amount - amount) + maximum_sliver_for_free && selected_reclaim < this_reclaim)) {
					selected = block.slotid;
					selected_reclaim = this_reclaim;
				}
			}

			if (!selected_reclaim) return amount;
			if (!std::forward<Handler>(by_doing)(selected)) {
				ESP_LOGW(TAG, "free_space_matching handler returned false, stopping early.");
				return amount;
			}
			else amount += selected_reclaim;
		}

		return amount;
	}

	void DataUpdateManager::inform_temp_change(uint16_t slotid, uint8_t temp) {
		// Inform STM TODO: retry until ack'd
		slots::PacketWrapper<3> pkt;
		pkt.init(slots::protocol::DATA_TEMP);
		pkt.put(slotid, 0);
		pkt.data()[2] = temp;
		serial::interface.send_pkt(pkt);
	}

	void DataUpdateManager::perform_warm_ephemeral_reclaim(StmMemoryBudgetInfo &budget, size_t amount, uint16_t ignoring_slotid) {
		// Try to free space
		budget.free_by(budget.allocated_warm_ephemeral, free_slots_matching_using(amount, [ignoring_slotid](auto& b){
			return b && b.temperature == bheap::Block::TemperatureWarm && b.location != bheap::Block::LocationRemote && b.slotid != ignoring_slotid;
		}, [&](uint16_t slotid){
			if (!arena.set_temperature(slotid, bheap::Block::TemperatureColdWantsWarm)) return false;

			// Tell the stm this has been made cold
			inform_temp_change(slotid, bheap::Block::TemperatureCold);

			// Update budget info for remotes
			std::for_each(arena.begin(slotid), arena.end(slotid), [&](auto &b){
				if (b.location == bheap::Block::LocationRemote) budget.xfer_into(budget.warm_remote, b.datasize, budget.cold_remote);
			});

			return true;
		}));
	}

	void DataUpdateManager::perform_cold_remote_reclaim(StmMemoryBudgetInfo &budget, size_t amount, uint16_t ignoring_slotid) {
		// Check maximum reclaimable region
		size_t reclaim = std::min<size_t>(budget.cold_remote, std::max<size_t>(0, arena.free_space() - target_free_space_buffer));
		if (!reclaim) return;
		if (reclaim > amount) reclaim = amount;

		budget.free_by(budget.cold_remote, free_space_matching_using(reclaim, [ignoring_slotid](auto& b){
			return b && b.temperature == bheap::Block::TemperatureCold && b.location == bheap::Block::LocationRemote && b.slotid != ignoring_slotid;
		}, [&](auto& blk, size_t len){
			
			return false;
		}));
	}

	DataUpdateManager::StmMemoryBudgetInfo DataUpdateManager::calculate_memory_budget() {
		auto budget = StmMemoryBudgetInfo{};
		budget.unused_space = STM_HEAP_SIZE - target_free_space_buffer;
		// Place each block into the right section
		for (const auto &b : arena) {
			// Is this a remote block?
			if (b.location == bheap::Block::LocationRemote) {
				switch (b.temperature) {
					case bheap::Block::TemperatureHot:
						if (arena.block_offset(b) != 0) continue;
						budget.use_for(budget.hot_remote, 4 + arena.contents_size(b.slotid));
						break;
					case bheap::Block::TemperatureWarm:
						budget.use_for(budget.warm_remote, 4 + b.rounded_datasize());
						break;
					default:
						budget.use_for(budget.cold_remote, 4 + b.rounded_datasize());
						break;
				}
			}
			else if (b.location == bheap::Block::LocationCanonical) {
				switch (b.temperature) {
					case bheap::Block::TemperatureHot:
						if (arena.block_offset(b) != 0) continue;
						budget.use_for(budget.hot_ephemeral, 4 + arena.contents_size(b.slotid));
						break;
					case bheap::Block::TemperatureWarm:
						budget.use_for(budget.allocated_warm_ephemeral, 4 + b.rounded_datasize());
						break;
					default:
						break;
				}
			}
		}
		ESP_LOGD(TAG, "membudget: %d free, %d hot, %d warm %d cold, %d remote", budget.unused_space, budget.hot(), budget.allocated_warm(), budget.cold_remote, budget.remote());
		return budget;
	}

	bool DataUpdateManager::ensure_budget_space(UseIntent reason, uint16_t slotid, ssize_t delta_size_change) {
		auto budget = calculate_memory_budget();
		return ensure_budget_space(reason, slotid, delta_size_change, budget);
	}

	void DataUpdateManager::try_reclaim_supressed_warm() {
		// Find how much space we have to work with
		auto budget = calculate_memory_budget();

		// Compute how much we could potentially reclaim.
		size_t supressed_blocks = 0;
		for (auto &b : arena) {
			if (b && b.location == bheap::Block::LocationCanonical && b.temperature == bheap::Block::TemperatureColdWantsWarm) supressed_blocks += 4 + b.datasize;
		}

		// Try to use unused space first
		for (const auto& startb : arena) {
			if (!startb || startb.temperature != bheap::Block::TemperatureColdWantsWarm || &startb != &arena.get(startb.slotid)) continue;
			// Check size
			auto sz = arena.contents_size(startb.slotid) + 4;
			// Does this fit in unused?
			if (sz > budget.unused_space) {
				// Try to clear cold
				if (sz <= budget.cold_remote) {
					perform_cold_remote_reclaim(budget, sz, startb.slotid);
					if (sz > budget.unused_space) continue;
				}
				else continue;
			}
			// Update in arena
			if (!arena.set_temperature(startb.slotid, bheap::Block::TemperatureWarm)) {
				ESP_LOGE(TAG, "# failed to set temp");
				return;
			}
			// Re-compute budget
			budget = calculate_memory_budget();
			// Send to the STM the new temp
			inform_temp_change(startb.slotid, bheap::Block::TemperatureWarm);
			// TODO await ack

			// Use temp handler to update
			DataUpdateRequest dur;
			dur.type = DataUpdateRequest::TypeSetTemp;
			dur.d_temp.slotid = startb.slotid;
			dur.d_temp.newtemperature = bheap::Block::TemperatureWarm;
			set_temp_handler(dur, false /* don't send ack */);
		}
	}

	bool DataUpdateManager::ensure_budget_space(UseIntent reason, uint16_t slotid, ssize_t delta_size_change, StmMemoryBudgetInfo& budget) {
		auto temp   = arena.contains(slotid) ? arena.get(slotid).temperature : bheap::Block::TemperatureCold;

		if (delta_size_change > 0) {
			switch (reason) {
				case UseForRemoteStorage:
					{
						// Allocate additional space into either hot/warm or the cold reserve area. If the block is warm, we 
						// place into warm_remote and if it's hot we put it into hot_remote.

						// First just check if there's enough space outright in the free area.
						if (delta_size_change <= budget.unused_space) {
							// There's already enough space, return now
							return true;
						}

						// Otherwise, start by allocating away all of the unused space. The STM heap used for calculating it is intentionally a bit smaller
						// so it's perfectly safe to use all of it
						delta_size_change -= std::exchange(budget.unused_space, 0);

						// Otherwise, check if there's any space to be gained from eliminating warm ephemerals. We do this first in this path
						// since the RemoteStorage implementation really shouldn't be trying to fill up our local heap. It will if it _must_, but
						// in general we try to avoid doing that.

						if (budget.allocated_warm_ephemeral) {
							perform_warm_ephemeral_reclaim(budget, delta_size_change);
						}

						if (delta_size_change <= budget.unused_space) {
							// There's now enough space, continue
							return true;
						}

						// If there's no more space here, we're probably screwed: this is called for allocating _remote storage_ which
						// should only occur when we're out of space for local storage, which means that we can't recover any cold remote blocks.
						//
						// That being said, this is _also_ used when allocating space for extra things on remoted warm or higher, so we'll also perform cold
						// reclaiming if the slot is warm or higher
						if (temp < bheap::Block::TemperatureWarm) return false;
						
common_remote_use_end:
						delta_size_change -= std::exchange(budget.unused_space, 0);

						if (budget.cold_remote) {
							perform_cold_remote_reclaim(budget, delta_size_change, slotid);
						}

						return delta_size_change <= budget.unused_space;
					}
				case UseForRemoteDisplay:
					{
						// This follows a somewhat different prioritization scheme to the storage case, since it's intended
						// to be called in a situation where there might _not_ be an active esp resource contention, hence trying to reclaim cold blocks is preferable.

						if (delta_size_change <= budget.unused_space) {
							return true;
						}

						delta_size_change -= std::exchange(budget.unused_space, 0);

						if (budget.cold_remote) {
							perform_cold_remote_reclaim(budget, delta_size_change, slotid);
						}

						// Still no space?
						if (delta_size_change <= budget.unused_space) {
							return true;
						}

						delta_size_change -= std::exchange(budget.unused_space, 0);
						
						// Try to free out warm blocks

						if (budget.allocated_warm_ephemeral) {
							perform_warm_ephemeral_reclaim(budget, delta_size_change);
						}

						// Enough space?
						if (delta_size_change <= budget.unused_space) {
							return true;
						}

						// Continue trying to free cold blocks _again_ in case more were made
						goto common_remote_use_end;
					}
				case TryForRemoteCache:
					{
						// only try to kill off cold remotes.
						if (delta_size_change <= budget.unused_space) {
							return true;
						}

						delta_size_change -= std::exchange(budget.unused_space, 0);

						if (budget.cold_remote) {
							perform_cold_remote_reclaim(budget, delta_size_change, slotid);
						}
						return delta_size_change <= budget.unused_space;
					}
					break;
			}
		}
		return false;
	}

	void DataUpdateManager::cleanout_esp_space() {
		// Try to ensure there's some free space by:
		// 	- defragging
		arena.defrag();
		if (arena.free_space() > target_free_space_buffer) return;
		ESP_LOGW(TAG, "cleanout_esp_space");
		// - moving things to the stm
		auto budget = calculate_memory_budget();
		auto needed_space = target_free_space_buffer - arena.free_space();
		auto ship_out_blk = [&](const bheap::Block& tgt, size_t subsection){
			// Ship out block
			for (int tries = 0; tries < 3; ++tries) {
				switch (single_store_fulfill(tgt.slotid, arena.block_offset(tgt), subsection, tgt.data(), true)) {
					case slots::protocol::DataStoreFulfillResult::Ok:
						// finish by setting block to remote
						arena.set_location(tgt.slotid, arena.block_offset(tgt), subsection, bheap::Block::LocationRemote);
						return true;
					case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_Failed:
					case slots::protocol::DataStoreFulfillResult::IllegalState:
					case slots::protocol::DataStoreFulfillResult::InvalidOrNak:
						ESP_LOGE(TAG, "bad state for block");
						return false;
					case slots::protocol::DataStoreFulfillResult::Timeout:
					case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_TryAgain:
						vTaskDelay(80);
						continue;
				}
			}
			return false;
		};
		// we prioritize moving warm/hot things since if they're hot/warm they're guaranteed to have space in the budget
		if (free_space_matching_using(needed_space, [](const bheap::Block& b){
			return b && b.temperature == bheap::Block::TemperatureWarm && b.location == bheap::Block::LocationCanonical;
		}, ship_out_blk) >= needed_space) return;
		// recompute budget
		budget = calculate_memory_budget();
		needed_space = target_free_space_buffer - arena.free_space();
		if (needed_space > budget.unused_space) {
			ESP_LOGE(TAG, "completely out of space in cleanout_esp_space; bailing.");
			return;
		}
		// ship out cold blocks too, as long as there's enough space for them.
		if (free_space_matching_using(needed_space, [](const bheap::Block& b){
			return b && b.temperature == bheap::Block::TemperatureCold && b.location == bheap::Block::LocationCanonical;
		}, ship_out_blk) < needed_space) {
			ESP_LOGE(TAG, "failed to free up enough space, bailing.");
		}
	}

	void DataUpdateManager::change_size_handler(DataUpdateRequest &dur) {
		// Change size does one of three things:
		// 	- if current size == target size, nothing at all.
		// 	- if current size < target size:
		// 		- act as if we're allocating a new canonical block at the end of the buffer
		// 	- if current size > target size:
		// 		- truncate the end of the block to compensate.
		//
		// 	In both of the last cases, we still have to inform the STM about the change, so it can do the same.
		// 	The difference on its implementation is that it allocates remote blocks instead of canonical/ephemeral.
		// 	We assume there's always enough space to handle storing remote placeholders, although for the most part they 
		// 	can get cleaned up if the slot in question isn't hot.

		// Check for the first case and exit early if possible.
		auto current_size = arena.contents_size(dur.d_chsize.slotid);
		auto original_size = current_size;
		if (current_size == dur.d_chsize.size) return;
		 
		auto new_blk_size = dur.d_chsize.size - current_size;
		
		const static size_t minimum_split_block_size = 64;

		// Inform the STM of the change
		{
			slots::PacketWrapper<4> pkt;
			pkt.init(slots::protocol::DATA_SET_SIZE);
			pkt.put(dur.d_chsize.slotid, 0);
			pkt.put(dur.d_chsize.size, 2);
			serial::interface.send_pkt(pkt);

			// Wait for an acknowledgement TODO proper retry logic here
			wait_for_packet([&](const slots::PacketWrapper<>& pw){return pw.cmd() == slots::protocol::ACK_DATA_SET_SIZE && memcmp(pkt.data(), pw.data(), 4) == 0;});
			finished_with_last_packet();
		}

		// Try to truncate
		if (current_size > dur.d_chsize.size) {
			arena.truncate_contents(dur.d_chsize.slotid, dur.d_chsize.size);
			// TODO: potentially make this reclaim reserved remote space?
			return;
		}
		else {
			// If there's not enough free space, try cleanup first.
			if (arena.contains(dur.d_chsize.slotid)) {
				// If this is already partially remoted and we're adding to a warm or higher, skip right to generating a remote block.
				if (std::any_of(arena.begin(dur.d_chsize.slotid), arena.end(dur.d_chsize.slotid), [](auto& b){return b.temperature >= bheap::Block::TemperatureWarm && b.location == bheap::Block::LocationRemote;}))
					goto only_send_remote;
				if (arena.free_space(arena.last(dur.d_chsize.slotid), arena.FreeSpaceAllocatable) < new_blk_size) arena.defrag();
			}
			// The "quick" path is to just allocate the block immediately, so we try that first.
			if (arena.add_block(dur.d_chsize.slotid, bheap::Block::LocationCanonical, new_blk_size)) {
				// It worked, so finish now
				return;
			}
			// There was no space for this block, and we've already tried a cleanup at this point, so we have to try further clearing.
			// Since we're _bound_ to allocate the new block on the STM, we're going to have to update it's size information first though, so handle this after that.
		}


		// Alright, now we can start allocating onto the STM
		ESP_LOGW(TAG, "out of space chsize, allocating...");

		// Before we do so, though, we might be getting an obscenely long update request which we could partially fit on our heap. There's a threshold here so we don't do
		// something dumb like split a 8 byte block into two 4 byte chunks.

		if (
			new_blk_size > minimum_split_block_size && // if there's enough block to split, and
			arena.free_space() > minimum_split_block_size + 4 + target_free_space_buffer // there's enough global free space (since defrag has already occured) to fit a block
		) {
			// Try and allocate part of the data on the heap
			if (arena.add_block(dur.d_chsize.slotid, bheap::Block::LocationCanonical, arena.free_space() - 4 - target_free_space_buffer)) {
				// Update current size due to allocated space
				current_size = arena.contents_size(dur.d_chsize.slotid);
				new_blk_size = dur.d_chsize.size - current_size;
			} // if this fails, we keep current_size as-is and just send the entire thing
		}

only_send_remote:
		// Ensure the remote placeholder will fit
		if (arena.free_space() < 4) {
			// Perform the bof™ technique for freeing up something. Unlike normal free this tries to get free space in _our_ heap.
			//
			// They share a common path though, which is trying to clear out "warm-buffer" space to fit.
			cleanout_esp_space(); // this function cleans out up to the target_free_space_buffer amount, which is guaranteed to be at least 4.
		}

		// Ensure there's enough space to dump this block
		if (!ensure_budget_space(UseForRemoteStorage, dur.d_chsize.slotid, new_blk_size + 4)) { // this un-warms blocks until there's enough "budget space" on the stm to fit the given parameter. the 
			ESP_LOGE(TAG, "Out of space trying to expand block, ignoring...");
			return;
		}
		// slotid is used to work out which region to allocate into.
		
		// At this point we should have enough space to put a remote block into our buffer _and_ enough space to send that to the stm, so do that now.
		if (!arena.add_block(dur.d_chsize.slotid, bheap::Block::LocationRemote, new_blk_size)) {
			ESP_LOGE(TAG, "Failed to allocate new space after ensuring budget space");
			return;
		}

		// Tell the STM that it's to store that data
		for (int retry = 0; retry < 2; ++retry) {
			switch (single_store_fulfill(dur.d_chsize.slotid, current_size, new_blk_size, nullptr, true)) {
				case slots::protocol::DataStoreFulfillResult::Ok:
					// Processed ok, return
					return;
				case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_Failed:
					ESP_LOGW(TAG, "out of space on stm from nef, treating as failed but should probably try to retrieve if possible.");
				case slots::protocol::DataStoreFulfillResult::IllegalState:
				case slots::protocol::DataStoreFulfillResult::InvalidOrNak:
					// Totally invalid, give up.
					break;
				case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_TryAgain:
					ESP_LOGW(TAG, "not enough space, awaiting cleanup");
					[[fallthrough]];
				case slots::protocol::DataStoreFulfillResult::Timeout:
					vTaskDelay(pdMS_TO_TICKS(5));
					continue;
			}
		}

		ESP_LOGE(TAG, "Failed to send to STM, cancelling update.");
		arena.truncate_contents(dur.d_chsize.slotid, original_size);
	}
};
