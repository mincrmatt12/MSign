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

	namespace {
		size_t round_with_max(size_t target, size_t maximum, size_t to) {
			// Round up by 64 bytes
			size_t base = target & ~(to - 1);
			if (target - base < (to/2)) target = base + to;
			else if (target == base) target = base;
			else target = base + to + to/2;
			if (target > maximum) return maximum;
			else return target;
		}
	}

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
		static slots::PacketWrapper<> nullp{};
		uint32_t v;
		if (!xTaskNotifyWait(0, 0xffff'ffff, &v, timeout)) {
			pending_packet_in = nullptr;
			nullp.size = 0;
			nullp.cmd_byte = 0;
			nullp.direction = 0;
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
		for (int include_warm = 0; include_warm <= 1; ++include_warm) {
			// Ship out all blocks that are >= Warm with the dirty flag set.
			for (auto& b : arena) {
				if (!(b.location == bheap::Block::LocationCanonical && b.temperature >= (include_warm ? bheap::Block::TemperatureWarm : bheap::Block::TemperatureHot) && b.flags & bheap::Block::FlagDirty)) continue;
				int spacefail = 0;
				for (int tries = 0; tries < 4; ++tries) {
					switch (single_store_fulfill(b.slotid, arena.block_offset(b), b.datasize, b.data(), false)) {
						case slots::protocol::DataStoreFulfillResult::Ok:
							goto block_ok;
						case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_Failed:
							spacefail = 2;
							[[fallthrough]];
						case slots::protocol::DataStoreFulfillResult::IllegalState:
						case slots::protocol::DataStoreFulfillResult::InvalidOrNak:
							ESP_LOGE(TAG, "error sending block.");
							goto block_fail;
						case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_TryAgain:
							spacefail = 1;
							[[fallthrough]];
						case slots::protocol::DataStoreFulfillResult::Timeout:
							vTaskDelay(40);
							continue;
					}
				}
block_fail:
				if (spacefail) {
					// not enough space for block, if it's warm set it to non-warm
					if (b.temperature == bheap::Block::TemperatureWarm) {
						ESP_LOGW(TAG, "out of space shipping out warm block, deferring slot...");
						inform_temp_change(b.slotid, bheap::Block::TemperatureCold);
						arena.set_temperature(b.slotid, spacefail == 1? bheap::Block::TemperatureColdWantsWarm : bheap::Block::TemperatureCold); // not the "wants warm" variant since this shouldn't happen normally.
					}
				}
				ESP_LOGE(TAG, "Failed to ship out block (%p) (%03x)", &b, b.slotid);
				continue;
block_ok:
				// mark block non-dirty
				b.flags &= ~bheap::Block::FlagDirty;
			}
		}
		sync_pending = false;
	}

	void DataUpdateManager::run() {
		dupm_task = xTaskGetCurrentTaskHandle();

		while (true) {
			// Wait for a request
			DataUpdateRequest dur;
			
			if (xQueueReceive(pending, &dur, sync_pending ? pdMS_TO_TICKS(50) : pdMS_TO_TICKS(1500)) == pdFALSE) {
				send_pending_dirty_blocks();
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
				// Is this a warm block? if so, just give up on sending it in general.
				if (b->temperature == bheap::Block::TemperatureWarm && dur.d_dirty.size > 8 && !(b->flags & bheap::Block::FlagDirty)) {
					ESP_LOGW(TAG, "detected warm block being marked dirty, marking it cold to prevent loops.");
					// Abandon updating this block at all.
					inform_temp_change(dur.d_dirty.slotid, bheap::Block::TemperatureCold);
					arena.set_temperature(dur.d_dirty.slotid, bheap::Block::TemperatureColdWantsWarm);
					return;
				}
				// Can we be more precise with this? Check if the block only partially contains the region
				auto current_offset = arena.block_offset(*b);
				auto before = dur.d_dirty.offset - current_offset;
				auto after = (dur.d_dirty.offset + dur.d_dirty.size) > (current_offset + b->datasize) ? 0 : (current_offset + b->datasize) - (dur.d_dirty.offset + dur.d_dirty.size);
				if (before + after > 256) {
					// If there's a significant difference, split block
					auto start_off = std::max<uint32_t>(dur.d_dirty.offset, current_offset);
					arena.ensure_single_block(dur.d_dirty.slotid, start_off, std::min<uint32_t>((current_offset + b->datasize), (dur.d_dirty.offset + dur.d_dirty.size)) - start_off);
					b = &arena.get(dur.d_dirty.slotid, start_off);
				}

				b->flags |= bheap::Block::FlagDirty;
			}
			else if (b->location == bheap::Block::LocationRemote) {
				ESP_LOGW(TAG, "Block %p has not yet been moved back to the esp, so we're ignoring the request to clear it", b);
			}
			else {
				ESP_LOGW(TAG, "Skipping %p, (%03x, %d, %d, %d)", b, b->slotid, b->temperature, b->location, b->datasize);
			}
		}

		sync_pending = true;
	}

	slots::protocol::DataStoreFulfillResult DataUpdateManager::single_store_fulfill(uint16_t slotid, uint16_t offset, uint16_t length, const void * datasource, bool is_store) {
		if (!length) return slots::protocol::DataStoreFulfillResult::Ok;
		{
			slots::PacketWrapper<255> pkt;
			pkt.init(is_store ? slots::protocol::DATA_STORE : slots::protocol::DATA_FULFILL);

			pkt.put(length, 4);
			size_t bytes_without_pause = 0;
			for (uint16_t suboff = 0; suboff < length; suboff += (255-6)) {
				if (bytes_without_pause > 768) { // ensure we don't send too much data too quickly, as this can confuse the STM and cause it to get stuck.
					vTaskDelay(pdMS_TO_TICKS(2));
					bytes_without_pause = 0;
				}
				bool start = suboff == 0;
				bool end   = suboff + (255-6) >= length;
				if (end) {
					pkt.size = 6 + length - suboff;
				}
				pkt.put<uint16_t>((start << 15) | (end << 14) | (slotid & 0xfff), 0);
				pkt.put<uint16_t>(suboff + offset, 2);
				if (datasource) memcpy(pkt.data() + 6, &((const uint8_t *)datasource)[suboff], pkt.size - 6);

				serial::interface.send_pkt(pkt);
				bytes_without_pause += pkt.size + 3;
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
						// Homogenize this block, this simplifies this output code
						arena.homogenize(dur.d_temp.slotid);
						// For all remote blocks, ship them out to the STM. We also perform this for warm, so only in the low-space condition does this happen _now_.
						for (auto b = arena.begin(dur.d_temp.slotid); b != arena.end(dur.d_temp.slotid); ++b) {
							if (b->location == bheap::Block::LocationRemote || b->datasize == 0) continue;
							// Ship out block
							for (int tries = 0; tries < 3; ++tries) {
								switch (single_store_fulfill(b->slotid, arena.block_offset(*b), b->datasize, b->data(), true)) {
									case slots::protocol::DataStoreFulfillResult::Ok:
										// finish by setting block to remote
										arena.set_location(b->slotid, arena.block_offset(*b), b->datasize, bheap::Block::LocationRemote);
										// mark not dirty
										b->flags &= ~bheap::Block::FlagDirty;
										goto ok;
									case slots::protocol::DataStoreFulfillResult::NotEnoughSpace_Failed:
										if (dur.d_temp.newtemperature == bheap::Block::TemperatureWarm) {
											ESP_LOGW(TAG, "out of space trying to send warm for %03x; deferring", dur.d_temp.slotid);
											arena.set_temperature(dur.d_temp.slotid, bheap::Block::TemperatureColdWantsWarm);
											// Send a cold
											inform_temp_change(dur.d_temp.slotid, bheap::Block::TemperatureCold);
										}
										[[fallthrough]];
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
							ESP_LOGE(TAG, "Failed to ship out remote block in Warm/Hot (%p)", &*b);
ok:
							;
						}
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
		std::is_invocable_r_v<ssize_t, Handler, bheap::Block&, size_t>,
	size_t> DataUpdateManager::free_space_matching_using(size_t target_amount, Pred&& from_blocks_matching, Handler&& by_doing) {
		if constexpr (std::is_same_v<std::invoke_result_t<Handler, bheap::Block&, size_t>, bool>) {
			// call with wrapper that assumes entire block is sent or not
			return free_space_matching_using(target_amount, std::forward<Pred>(from_blocks_matching), [sub=std::forward<Handler>(by_doing)](bheap::Block& b, size_t a){
				return sub(b, a) ? a : -1;
			});
		}
		else {
			size_t amount = 0, last_failed_amount = -1;
			while (amount < target_amount) {
				bheap::Block *selected = nullptr;
				for (auto& block : arena) {
					if (!std::forward<Pred>(from_blocks_matching)(block)) continue;
					if (block.datasize >= last_failed_amount) continue;
					if (!selected || (selected->datasize <= (target_amount - amount) + maximum_sliver_for_free && selected->datasize < block.datasize)) selected = &block;
				}

				// If there are no more blocks to free, return the amount we freed anyways
				if (!selected) return amount;

				size_t use_amount = (target_amount - amount);

				// Otherwise free:
				if (selected->datasize <= (target_amount - amount) + maximum_sliver_for_free) {
					use_amount = selected->datasize;
				}

				if (use_amount > last_failed_amount) {
					use_amount = last_failed_amount;
				}

				if (auto amt = std::forward<Handler>(by_doing)(*selected, use_amount); amt != -1) amount += amt;
				else {
					last_failed_amount = use_amount;
					ESP_LOGW(TAG, "free_space handler failed, limiting to %d", last_failed_amount);
					if (last_failed_amount < 8) return amount;
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
				if (!block || &block != &arena.get(block.slotid) || !arena.contents_size(block.slotid)) continue;
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
		// Only minimal rounding is performed here because there isn't much point in overcompensating
		amount = round_with_max(amount, budget.allocated_warm_ephemeral, 32);
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
		size_t reclaim_max = std::min<size_t>(budget.cold_remote, std::max<size_t>(0, arena.free_space() - target_free_space_buffer));
		if (!reclaim_max) return;
		auto reclaim = round_with_max(amount, reclaim_max, 128);
		// Round up to avoid small chunks

		ESP_LOGD(TAG, "cold_reclaim: reclaim %zu; amount %zu", reclaim, amount);

		budget.free_by(budget.cold_remote, free_space_matching_using(reclaim, [ignoring_slotid](auto& b){
			return b && b.temperature <= bheap::Block::TemperatureColdWantsWarm && b.location == bheap::Block::LocationRemote && b.slotid != ignoring_slotid;
		}, [&](bheap::Block& blk, size_t len) -> bool {
			auto orig_offset = arena.block_offset(blk), slotid = blk.slotid;
			ESP_LOGD(TAG, "reclaiming %03x @ %04d // %04zu", slotid, orig_offset, len);
			// Make new space for this block
			if (!arena.set_location(slotid, orig_offset, len, bheap::Block::LocationCanonical)) {
				ESP_LOGD(TAG, "out of space making canonical in reclaim");
				arena.defrag();
				if (!arena.set_location(slotid, orig_offset, len, bheap::Block::LocationCanonical)) {
					ESP_LOGW(TAG, "Really out of space..");
					return false;
				}
			}
			
			// Ask the STM for this data.
			{
				slots::PacketWrapper<6> pkt;
				pkt.init(slots::protocol::DATA_RETRIEVE);
				pkt.put<uint16_t>(slotid, 0);
				pkt.put<uint16_t>(orig_offset, 2);
				pkt.put<uint16_t>(len, 4);

				serial::interface.send_pkt(pkt);

				// Wait for a corresponding ack
				if (!wait_for_packet([&](const slots::PacketWrapper<0>& pw){return pw.cmd() == slots::protocol::ACK_DATA_RETRIEVE && memcmp(pkt.data(), pw.data(), 6) == 0;}, pdMS_TO_TICKS(200))) return false;
				finished_with_last_packet();
			}

			// Await move commands
			while (true) {
				auto& pkt = wait_for_packet([&](const slots::PacketWrapper<0>& pw){return pw.cmd() == slots::protocol::DATA_STORE && (pw.template at<uint16_t>(0) & 0xfff) == slotid;}, pdMS_TO_TICKS(500));
				if (!pkt) {
					ESP_LOGW(TAG, "timeout waiting for data_store");
					arena.set_location(slotid, orig_offset, len, bheap::Block::LocationRemote);
					return false;
				}

				uint16_t sid_frame = pkt.template at<uint16_t>(0);
				uint16_t offset    = pkt.template at<uint16_t>(2);
				uint16_t total_len = pkt.template at<uint16_t>(4);

				if (total_len != len) ESP_LOGW(TAG, "still processing weird-lengthed update");
				// verify start makes sense
				if (sid_frame & (1 << 15)) {
					if (offset != orig_offset) ESP_LOGW(TAG, "still processing update with incorrect position");
				}
				// update data -- this sets dirty but that's fine, we'll account for it
				arena.update_contents(slotid, offset, pkt.size - 6, pkt.data() + 6);
				finished_with_last_packet();
				// finish on end
				if (sid_frame & (1 << 14)) {
					ESP_LOGD(TAG, "got update ok");
					break;
				}
			}

			// Send an ACK
			{
				slots::PacketWrapper<7> pkt;
				pkt.init(slots::protocol::ACK_DATA_STORE);
				pkt.put<uint16_t>(slotid, 0);
				pkt.put<uint16_t>(orig_offset, 2);
				pkt.put<uint16_t>(len, 4);
				pkt.data()[6] = (uint8_t)slots::protocol::DataStoreFulfillResult::Ok;

				serial::interface.send_pkt(pkt);
			}
			return true;
		}));
	}

	DataUpdateManager::StmMemoryBudgetInfo DataUpdateManager::calculate_memory_budget() {
		auto budget = StmMemoryBudgetInfo{};
		uint32_t cs = 0;
		budget.unused_space = STM_HEAP_SIZE - target_free_space_buffer;
		// Place each block into the right section
		for (const auto &b : arena) {
			if (!b) continue;
			// Is this a remote block?
			if (b.location == bheap::Block::LocationRemote) {
				switch (b.temperature) {
					case bheap::Block::TemperatureHot:
						if (&b != &arena.get(b.slotid)) continue;
						cs = arena.contents_size(b.slotid); if (cs % 4) cs += 4 - (cs % 4);
						budget.use_for(budget.hot_remote, 4 + cs);
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
						if (&b != &arena.get(b.slotid)) continue;
						cs = arena.contents_size(b.slotid); if (cs % 4) cs += 4 - (cs % 4);
						budget.use_for(budget.hot_ephemeral, 4 + cs);
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
redo_loop:
		for (const auto& startb : arena) {
			if (!startb || startb.temperature != bheap::Block::TemperatureColdWantsWarm || &startb != &arena.get(startb.slotid)) continue;
			// Check size
			auto sz = arena.contents_size(startb.slotid) + 4;
			// Does this fit in unused?
			if (sz > budget.unused_space) {
				// Try to clear cold
				if (sz <= budget.cold_remote) {
					perform_cold_remote_reclaim(budget, sz, startb.slotid);
					if (sz > budget.unused_space) return;
					goto redo_loop;
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
				case TryForLocalDedup:
					{
						uint16_t local_free_space = arena.free_space() - target_free_space_buffer;
						// Try to free local space by moving first entire warm, then entire hot, then partial warm to the stm completely,
						// avoiding duplication of data at the expense of slower local updates.

						if (delta_size_change <= local_free_space) {
							return true;
						}

						// Try to move warm slots.
						if (budget.allocated_warm_ephemeral) {
							local_free_space += budget.xfer_into(budget.allocated_warm_ephemeral, free_slots_matching_using(round_with_max(delta_size_change, budget.allocated_warm_ephemeral, 64),
								[&](const bheap::Block& b){
									return b && b.temperature == bheap::Block::TemperatureWarm && b.location == bheap::Block::LocationCanonical;
								}, [&](uint16_t slotid){
									// Move all blocks in this slot.
									return std::all_of(arena.begin(slotid), arena.end(slotid), [&](auto& blk){
										return blk.location == bheap::Block::LocationRemote || move_block_to_stm(blk, blk.datasize);
									});
								}
							), budget.warm_remote);
						}

						// Did we free up enough space?
						if (delta_size_change <= local_free_space) {
							return true;
						}

						// Use up space as much as possible
						delta_size_change -= std::exchange(local_free_space, 0);

						// Now try allocating hot blocks
						if (budget.hot_ephemeral) {
							local_free_space += budget.xfer_into(budget.hot_ephemeral, free_slots_matching_using(round_with_max(delta_size_change, budget.hot_ephemeral, 64),
								[&](const bheap::Block& b){
									return b && b.temperature == bheap::Block::TemperatureHot && b.location == bheap::Block::LocationCanonical;
								}, [&](uint16_t slotid){
									// Move all blocks in this slot.
									return std::all_of(arena.begin(slotid), arena.end(slotid), [&](auto& blk){
										return blk.location == bheap::Block::LocationRemote || move_block_to_stm(blk, blk.datasize);
									});
								}
							), budget.hot_remote);
						}

						return local_free_space >= delta_size_change;
					}
					break;
			}
		}
		return false;
	}

	bool DataUpdateManager::move_block_to_stm(const bheap::Block& tgt, size_t subsection_length) {
		ESP_LOGD(TAG, "moving block %p (%03x, %d) // %d to stm", &tgt, tgt.slotid, tgt.datasize, subsection_length);
		// Ship out block
		for (int tries = 0; tries < 3; ++tries) {
			switch (single_store_fulfill(tgt.slotid, arena.block_offset(tgt), subsection_length, tgt.data(), true)) {
				case slots::protocol::DataStoreFulfillResult::Ok:
					ESP_LOGD(TAG, "moved block ok");
					// finish by setting block to remote
					arena.set_location(tgt.slotid, arena.block_offset(tgt), subsection_length, bheap::Block::LocationRemote);
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
	}

	void DataUpdateManager::cleanout_esp_space() {
		// Try to ensure there's some free space by:
		// 	- defragging
		if (arena.free_space() > target_free_space_buffer) return;
		arena.defrag();
		if (arena.free_space() > target_free_space_buffer) return;
		ESP_LOGW(TAG, "cleanout_esp_space");
		// - moving things to the stm
		auto budget = calculate_memory_budget();
		auto needed_space = target_free_space_buffer - arena.free_space();
		auto ship_out_blk = [&](const auto& blk, size_t len){return move_block_to_stm(blk, len);};
		// we prioritize moving warm/hot things since if they're hot/warm they're guaranteed to have space in the budget
		if (free_space_matching_using(needed_space, [](const bheap::Block& b){
			return b && b.temperature == bheap::Block::TemperatureWarm && b.location == bheap::Block::LocationCanonical && b.datasize;
		}, ship_out_blk) >= needed_space) {
			arena.defrag();
			return;
		}
		// recompute budget
		budget = calculate_memory_budget();
		needed_space = target_free_space_buffer - arena.free_space();
		if (needed_space > budget.unused_space) {
			ESP_LOGE(TAG, "completely out of space in cleanout_esp_space; bailing.");
			return;
		}
		// ship out cold blocks too, as long as there's enough space for them.
		if (free_space_matching_using(needed_space, [](const bheap::Block& b){
			return b && b.temperature == bheap::Block::TemperatureCold && b.location == bheap::Block::LocationCanonical && b.datasize;
		}, ship_out_blk) < needed_space) {
			ESP_LOGE(TAG, "failed to free up enough space, bailing.");
		}
		arena.defrag();
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

		int alloc_attempts = 0;

		bool needs_to_defer_to_remote = false, done_ok = false;

		// Try to truncate
		if (current_size > dur.d_chsize.size) {
			arena.truncate_contents(dur.d_chsize.slotid, dur.d_chsize.size);
			try_reclaim_supressed_warm();
			done_ok = true;
		}
		else {
			if (arena.contains(dur.d_chsize.slotid)) {
				// If the slot is already present, we have to check where we should try to allocate, as well as whether or not this allocation will run out of space.
				//
				// If the slot is cold (incl. wantswarm) (or not present):
				// 		- allocate on esp if possible, otherwise defer to stm.
				// 	           is warm:
				// 	    - allocate:
				// 	    	- on esp if there's space and the slot is not already partially on the stm
				// 	    	- on stm if there isn't space on esp or if some of the slot is already on the stm
				// 	    		note: calculations for available space in esp may be changed.
				// 	    - use ensure_budget_space with TryForRemoteCache to make sure there's enough space on stm for slot to continue being warm if we are allocating a non-remote
				// 	      chunk
				// 	        - if not, set block to cold
				// 	    "   "  is hot:
				// 	    - slot _must_ have enough space, so do the same as warm but with remotedisplay instead, and if the allocation fails ignore the change size requestand
				// 	      complain loudly.
				//
				auto current_temp = arena.get(dur.d_chsize.slotid).temperature;
				needs_to_defer_to_remote = std::any_of(arena.begin(dur.d_chsize.slotid), arena.end(dur.d_chsize.slotid), [](auto& b){return b.location == bheap::Block::LocationRemote;});

				if (current_temp == bheap::Block::TemperatureWarm) {
					// Ensure there's enough space for this block.
					if (!ensure_budget_space(TryForRemoteCache, dur.d_chsize.slotid, new_blk_size)) {
						// If there isn't enough space, set this block back to cold.
						inform_temp_change(dur.d_chsize.slotid, bheap::Block::TemperatureCold);
						arena.set_temperature(dur.d_chsize.slotid, bheap::Block::TemperatureColdWantsWarm);
					}
				}
				else if (current_temp == bheap::Block::TemperatureHot) {
					// Ensure there's enough space, bailing early if there isn't
					if (!ensure_budget_space(UseForRemoteDisplay, dur.d_chsize.slotid, new_blk_size)) {
						ESP_LOGE(TAG, "not enough space for hot block (%03x, adding %d bytes)", dur.d_chsize.slotid, new_blk_size);
						return; // don't process this change.
					}
				}
				else {
					needs_to_defer_to_remote = false;
				}
			}

			// If we're trying to allocate on the esp, do a defrag.
			if (!needs_to_defer_to_remote) {
				if (arena.free_space(arena.last(dur.d_chsize.slotid)) < new_blk_size + 4) arena.defrag();

				if (arena.add_block(dur.d_chsize.slotid, bheap::Block::LocationCanonical, new_blk_size)) {
					done_ok = true;
					// Make sure there's enough space going forwards
					cleanout_esp_space();
				}
				else {
					ESP_LOGW(TAG, "not enough space allocating for %03x, moving", dur.d_temp.slotid);
				}

				// There was no space for this block, and we've already tried a cleanup at this point, so we have to try further clearing.
				// Since we're _bound_ to allocate the new block on the STM, we're going to have to update it's size information first though, so handle this after that.
			}
		}

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

		if (done_ok) return;

		// Alright, now we can start allocating onto the STM
		ESP_LOGW(TAG, "out of space chsize, allocating...");

		// Before we do so, though, we might be getting an obscenely long update request which we could partially fit on our heap. There's a threshold here so we don't do
		// something dumb like split a 8 byte block into two 4 byte chunks.

retry_allocation:
		if (
			!needs_to_defer_to_remote &&
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

		// Ensure there's enough space to dump this block
		if (!ensure_budget_space(UseForRemoteStorage, dur.d_chsize.slotid, new_blk_size + 4) || arena.free_space() < 4) { // this un-warms blocks until there's enough "budget space" on the stm to fit the given parameter. the 
			if (needs_to_defer_to_remote) {
				ESP_LOGE(TAG, "Out of space allocating space in hot block, bailing.");
				return;
			}
			ESP_LOGW(TAG, "Out of space moving block to STM, trying to remove duplicates and place locally.");
			if (ensure_budget_space(TryForLocalDedup, dur.d_chsize.slotid, new_blk_size + 4)) {
				// We've created enough space locally, just do that instead.
				if (arena.add_block(dur.d_chsize.slotid, bheap::Block::LocationCanonical, new_blk_size)) {
					return;
				}
			}
			// If we failed to allocate or failed to ensure space, we might have still made enough space that after splitting it will work. Loop back to the split code.
			if (alloc_attempts++ == 0) goto retry_allocation;
			return;
		}

		// Ensure the remote placeholder will fit
		if (arena.free_space() < 32) {
			// Perform the bof™ technique for freeing up something. Unlike normal free this tries to get free space in _our_ heap.
			//
			// They share a common path though, which is trying to clear out "warm-buffer" space to fit.
			cleanout_esp_space(); // this function cleans out up to the target_free_space_buffer amount, which is guaranteed to be at least 4.
		}
		
		// At this point we should have enough space to put a remote block into our buffer _and_ enough space to send that to the stm, so do that now.
		if (!arena.add_block(dur.d_chsize.slotid, bheap::Block::LocationRemote, new_blk_size)) {
			ESP_LOGE(TAG, "Failed to allocate new space after ensuring budget space");
			return;
		}

		// Tell the STM that it's to store that data
		for (int retry = 0; retry < 3; ++retry) {
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

		// Make the STM aware we cancelled the update so it doesn't send us requests for data.
		{
			slots::PacketWrapper<4> pkt;
			pkt.init(slots::protocol::DATA_SET_SIZE);
			pkt.put(dur.d_chsize.slotid, 0);
			pkt.put(original_size, 2);
			serial::interface.send_pkt(pkt);

			// Wait for an acknowledgement TODO proper retry logic here
			wait_for_packet([&](const slots::PacketWrapper<>& pw){return pw.cmd() == slots::protocol::ACK_DATA_SET_SIZE && memcmp(pkt.data(), pw.data(), 4) == 0;});
			finished_with_last_packet();
		}
	}
};
