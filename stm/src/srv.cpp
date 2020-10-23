#include "srv.h"

#include "common/slots.h"
#include "tasks/timekeeper.h"
#include <string.h>
#include "stm32f2xx_ll_usart.h"
#include "stm32f2xx_ll_system.h"
#include "common/bootcmd.h"
#include "common/util.h"
#include "pins.h"
#include "nvic.h"
#include <algorithm>
#include <unistd.h>
#include <stdio.h>

#include <semphr.h>

#define FLASH_KEY1 0x45670123U
#define FLASH_KEY2 0xCDEF89ABU

extern tasks::Timekeeper timekeeper;

constexpr uint16_t srv_reclaim_low_watermark = 768;
constexpr uint16_t srv_reclaim_high_watermark_simple = 1636;
constexpr uint16_t srv_reclaim_high_watermark_complex = 1152;

#define USTATE_WAITING_FOR_READY 0
#define USTATE_SEND_READY 20
#define USTATE_WAITING_FOR_FINISH_SECTORCOUNT 21
#define USTATE_WAITING_FOR_FINISH_WRITE 22
#define USTATE_ERASING_BEFORE_IMAGE 1
#define USTATE_WAITING_FOR_PACKET 2
#define USTATE_PACKET_WRITTEN 3
#define USTATE_ERASING_BEFORE_PACKET 4
#define USTATE_FAILED 10
#define USTATE_PACKET_WRITE_FAIL_CSUM 11
// not the dish soap
#define USTATE_WAITING_FOR_FINISH 12

// Updating routines

uint32_t update_package_data_ptr;
uint32_t update_package_data_counter = 0;
uint8_t update_package_sector_counter = 5;

void begin_update(uint8_t &state) {
	// Set data_ptr to the beginning of the update memory area.
	
	update_package_data_ptr = 0x0808'0000;
	update_package_sector_counter = 8; // sector 8 is currently being written into.
	update_package_data_counter = 0;

	// Unlock FLASH
	
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;

	// Erase sector 8
	
	CLEAR_BIT(FLASH->CR, FLASH_CR_SNB);
	FLASH->CR |= FLASH_CR_SER /* section erase */ | (update_package_sector_counter << FLASH_CR_SNB_Pos);

	// Actually erase
	
	FLASH->CR |= FLASH_CR_STRT;

	state = USTATE_ERASING_BEFORE_IMAGE;
}

void append_data(uint8_t &state, uint8_t * data, size_t amt, bool already_erased=false) {
	if (!util::crc_valid(data, amt + 2)) {
		state = USTATE_PACKET_WRITE_FAIL_CSUM;
		return;
	}

	// Append data to counter
	if ((update_package_data_counter + amt) >= 131072 && !already_erased) {
		++update_package_sector_counter;

		// Erase the sector

		CLEAR_BIT(FLASH->CR, FLASH_CR_SNB);
		FLASH->CR |= FLASH_CR_SER /* section erase */ | (update_package_sector_counter << FLASH_CR_SNB_Pos);

		// Actually erase
		
		FLASH->CR |= FLASH_CR_STRT;

		state = USTATE_ERASING_BEFORE_PACKET;

		return;
	}
	else if (already_erased) {
		// we've just erased, so set the data counter to the amount in this new section
		update_package_data_counter = (update_package_data_counter + amt) - 131072;
	}
	else {
		// Otherwise, just program the data
		update_package_data_counter += amt;
	}

	CLEAR_BIT(FLASH->CR, FLASH_CR_PSIZE); // set psize to 0; byte by byte access

	while (amt--) {
		FLASH->CR |= FLASH_CR_PG;
		(*(uint8_t *)(update_package_data_ptr++)) = *(data++); // Program this byte

		// Wait for busy
		while (READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
			asm volatile ("nop");
			asm volatile ("nop");
			asm volatile ("nop");
			asm volatile ("nop");
		}

		// Program next byte
	}

	// Clear pg byte
	
	CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

	state = USTATE_PACKET_WRITTEN;
}

void srv::Servicer::set_temperature(uint16_t slotid, uint32_t temp) {
	if (arena.get(slotid) && arena.get(slotid).temperature == temp) return;
	PendRequest pr;
	pr.type = PendRequest::TypeChangeTemp;
	pr.slotid = slotid;
	pr.temperature = temp;
	xQueueSendToBack(pending_requests, &pr, portMAX_DELAY);
}

bool srv::Servicer::slot_dirty(uint16_t slotid, bool clear) {
	bool is_dirty = std::any_of(arena.begin(slotid), arena.end(slotid), [](const auto& b){return b.flags & bheap::Block::FlagDirty;});
	if (is_dirty && clear) {
		for (auto it = arena.begin(slotid); it != arena.end(slotid); ++it) {
			it->flags &= ~bheap::Block::FlagDirty;
		}
	}
	return is_dirty;
}


const bheap::Block& srv::Servicer::_slot(uint16_t slotid) {
	const bheap::Block* target;
	if (bcache.contains(slotid)) {
		target = reinterpret_cast<const bheap::Block*>(&arena.region[bcache.lookup(slotid) * 4]);
	}
	else {
		target = &arena.get(slotid);
		if (*target && !target->next())
			bcache.insert(slotid, static_cast<uint16_t>(reinterpret_cast<ptrdiff_t>(target - &arena.first)));
		else
			target = &arena.get(bheap::Block::SlotEnd);
	}
	return *target;
}

bool srv::Servicer::do_bheap_cleanup(bool full_cleanup) {
	// Do periodic/required cleanup:
	//
	// This function returns true if it sent a packet.
	
	// Start by checking how much free allocatable space we have
	
	auto current_space = arena.free_space();

	// If we more space than the low watermark + the total last update delta we don't do anything
	if (current_space > (srv_reclaim_low_watermark + last_update_failed_delta_size)) {
		last_update_failed_delta_size = 0;
		return false;
	}

	// TODO: use high_watermark_complex when checking for defrags
	int to_free = static_cast<int>(srv_reclaim_high_watermark_simple - current_space);

	srv::ServicerLockGuard g(*this); // Lock the buffer

	// First just try coalescing stuff
	if (arena.free_space(arena.FreeSpaceDefrag) - current_space >= to_free) {
		arena.defrag();
		bcache.evict();

		last_update_failed_delta_size = 0;
		return false;
	}

	// Otherwise we begin freeing up blocks.
	// This first pass will eliminate all non-cached cold ephemeral blocks
	//
	// We free from left to right; this could probably be more efficient going right to left but it'd use more memory more of the time.
	for (auto& block : arena) {
		if (block && block.location == bheap::Block::LocationEphemeral && block.datasize && block.temperature == bheap::Block::TemperatureCold && !bcache.contains(block.slotid)) {
			// Delete this block (shrink to size = 0 to create empty set location to remote and reset datasize)
			auto len = block.datasize;
			arena.set_location(block.slotid, arena.block_offset(block), block.datasize, bheap::Block::LocationRemote);

			auto freed = block.datasize - 4 + (block.datasize % 4 ? 0 : 4 - block.datasize % 4);
			to_free -= freed;
			current_space += freed;
		}

		if (to_free <= 0) {
			last_update_failed_delta_size = 0;
			return false;
		}
	}

	// We now do a cleanup to try and reclaim space if it'll recover enough on its own
	if (arena.free_space(arena.FreeSpaceDefrag) - current_space >= to_free) {
		arena.defrag();
		bcache.evict();

		last_update_failed_delta_size = 0;
		return false;
	}

	// Otherwise we start clearing out _all_ cold blocks.
	for (auto& block : arena) {
		if (block && block.location == bheap::Block::LocationEphemeral && block.datasize && block.temperature == bheap::Block::TemperatureCold) {
			// Delete this block (shrink to size = 0 to create empty set location to remote and reset datasize)
			auto len = block.datasize;
			arena.set_location(block.slotid, arena.block_offset(block), block.datasize, bheap::Block::LocationRemote);

			auto freed = block.datasize - 4 + (block.datasize % 4 ? 0 : 4 - block.datasize % 4);
			to_free -= freed;
			current_space += freed;
		}

		if (to_free <= 0) {
			last_update_failed_delta_size = 0;
			return false;
		}
	}

	// .. followed by all _warm_ blocks not in the cache
	for (auto& block : arena) {
		if (block && block.location == bheap::Block::LocationEphemeral && block.datasize && block.temperature == bheap::Block::TemperatureWarm && !bcache.contains(block.slotid)) {
			// Delete this block (shrink to size = 0 to create empty set location to remote and reset datasize)
			auto len = block.datasize;
			block.shrink(0);
			arena.set_location(block.slotid, arena.block_offset(block), block.datasize, bheap::Block::LocationRemote);

			auto freed = block.datasize - 4 + (block.datasize % 4 ? 0 : 4 - block.datasize % 4);
			to_free -= freed;
			current_space += freed;
		}

		if (to_free <= 0) {
			last_update_failed_delta_size = 0;
			return false;
		}
	}

	// We then _again_ run defrag uncoditionally
	arena.defrag();
	bcache.evict();

	to_free -= (arena.free_space() - current_space);
	current_space = arena.free_space();

	// At this point we're forced to start evicting blocks, so we bail out in the case where we're not allowed to start evicting anything.
	if (to_free <= 0) {
		last_update_failed_delta_size = 0;
		return false;
	}
	else if (!full_cleanup) {
		return false;
	}

	// Otherwise, try and find a good block to evict.
	
	// The best candidate is a cold block, so find the smallest cold block first
	bheap::Block *best_option = nullptr;
	for (auto &t : arena) {
		if (t && t.location == bheap::Block::LocationCanonical && t.temperature == bheap::Block::TemperatureCold) {
			if (best_option == nullptr) best_option = &t;
			else if (best_option->datasize >= t.datasize) best_option = &t; // >= to get the farthest right one (messing with stuff on the left is harder)
		}
	}

	// If we have a candidate, evict it
	if (best_option) {
		evict_block(*best_option);
		return true;
	}

	// Otherwise, try to evict a warm block.
	for (auto &t : arena) {
		if (t && t.location == bheap::Block::LocationCanonical && t.temperature == bheap::Block::TemperatureWarm) {
			if (best_option == nullptr) best_option = &t;
			else if (best_option->datasize >= t.datasize) best_option = &t; // >= to get the farthest right one (messing with stuff on the left is harder)
		}
	}

	// If we have a candidate, evict it
	if (best_option) {
		evict_block(*best_option);
		return true;
	}

	// Otherwise, we just give up and stop trying to clear 
	last_update_failed_delta_size = 0;
	return false;
}

void srv::Servicer::evict_block(bheap::Block& block) {
	uint16_t full_slot_size = arena.contents_size(block.slotid);
	uint16_t offset = arena.block_offset(block);
	uint16_t sid_frame = block.slotid | (0b1100 << 12);
	uint16_t total_upd_len = std::min<uint16_t>(56, block.datasize);

	wait_for_not_sending();

	dma_out_buffer[0] = 0xa5;
	dma_out_buffer[1] = total_upd_len + 8;
	dma_out_buffer[2] = slots::protocol::DATA_MOVE;

	*reinterpret_cast<uint16_t *>(dma_out_buffer + 3) = sid_frame;
	*reinterpret_cast<uint16_t *>(dma_out_buffer + 5) = offset;
	*reinterpret_cast<uint16_t *>(dma_out_buffer + 7) = full_slot_size;
	*reinterpret_cast<uint16_t *>(dma_out_buffer + 9) = total_upd_len;
	memcpy(dma_out_buffer + 11, block.data(), total_upd_len);

	send();
}

void srv::Servicer::do_handshake() {
	int retries = 0;

	// Check if the system has just updated
	if (bootcmd_did_just_update()) {
		// We did! Send out an update state finished
		dma_out_buffer[0] = 0xa5;
		dma_out_buffer[1] = 0x01;
		dma_out_buffer[2] = 0x63;
		dma_out_buffer[3] = 0x21;

		update_state = USTATE_WAITING_FOR_FINISH;
		update_chunks_remaining = 0; // used separately
		send();
		is_updating = true;
		start_recv(); // start receieving packets, will implicitly setup the state correctly. return to main loop to go to actual logic
		return;
	}
	else {
retry_handshake:
		// Send out the HANDSHAKE_INIT command.
		dma_out_buffer[0] = 0xa5;
		dma_out_buffer[1] = 0x00;
		dma_out_buffer[2] = 0x10;
		send();
		start_recv(ProtocolState::HANDSHAKE_RECV);
	}

	// Wait for the state to not be handshake_recv. Check every few ms
	// TODO: this could use task notifications but /shrug may as well keep old code around

	while (state == ProtocolState::HANDSHAKE_RECV) {
		vTaskDelay(pdMS_TO_TICKS(500));
		if (!is_sending) ++retries;

		if (retries == 3 || retries == 6 || retries == 9) {
			// Try sending it again
			goto retry_handshake;
		}

		if (retries == 12) {
			// Just reset at that point
			NVIC_SystemReset();
		}
	}

	// Send the actual handshake ok command and finish this process; or go into the update system
	switch (state) {
		case ProtocolState::HANDSHAKE_SENT:
			{
				// Send out the HANDSHAKE_OK command.
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x00;
				dma_out_buffer[2] = 0x12;
				send();


				// clear the overrun flag
				LL_USART_ClearFlag_ORE(ESP_USART); // this should really be moved elsewhere
				start_recv();
				return;
			}
		case ProtocolState::UPDATE_STARTING:
			{
				// Enter update mode, and tell the ESP we did
				
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x01;
				dma_out_buffer[2] = 0x63;
				dma_out_buffer[3] = 0x10; // UPDATE_STATUS; cmd=update mode entered

				send();
				is_updating = true; // Mark update mode.
				start_recv(); // Begin recieveing messages
			}
			return;
		default:
			++retries;
			vTaskDelay(pdMS_TO_TICKS(500));
			goto retry_handshake;
	}
}

void srv::Servicer::run() {
	// Initialize our task handle
	this_task = xTaskGetCurrentTaskHandle();

	// Do the handshake
	do_handshake();

	// If in update mode, go to the update logic handlers
	if (this->is_updating) {
		while (true)
			do_update_logic();
	}

	union {
		uint8_t msgbuf[16];
		uint16_t msgbuf_x16[8];
	};
	PendRequest active_request{};
	int active_request_send_retries = 0;
	bool is_cleaning = false;
	uint8_t move_update_errcode = 0;
	uint8_t active_request_statemachine = 0;

	// Otherwise, begin processing packets/requests
	while (true) {
		// Wait for a packet to come in over DMA. If this returns 0 (i.e. no packet but we still interrupted) then check the queue for a new task.
		//
		// This loop is effectively managing two separate tasks, one static - normal packets - and one that changes based on the top of the queue.
		// Additionally, a queue event will only ever cause a write and "wait" for a read in response.

		auto amount = xStreamBufferReceive(dma_rx_queue, msgbuf, 3, pdMS_TO_TICKS(400)); // every 100 ms we also try and look for cleanup-problems
		// Check for a new queue operation if we're not still processing one.
		if (amount < 3) {
			if (active_request.type == PendRequest::TypeNone) {
				// Check for new queue operation
				if (xQueueReceive(pending_requests, &active_request, 0)) {
					// Start handling it.
					active_request_send_retries = 0;
					start_pend_request(active_request);
				}
				// Otherwise, do various random tasks.
				else {
					check_connection_ping();
					update_forgot_statuses();

					if (xStreamBufferBytesAvailable(log_out) > 100) {
						// Do a flush
						active_request_send_retries = 0;
						start_pend_request(PendRequest{
							.type = PendRequest::TypeDumpLogOut
						});
					}

					if (!is_cleaning) is_cleaning = do_bheap_cleanup();
				}
			}
			else {
				check_connection_ping();

				++active_request_statemachine;
				if (active_request_statemachine == 2) {
					active_request_statemachine = 0;
					++active_request_send_retries;
					if (active_request_send_retries > 2) {
						// Failed to do a request
						active_request_send_retries = 0;
						// Consume this request.
						active_request.type = PendRequest::TypeNone;
						// Try tail-chaining it
						if (xQueueReceive(pending_requests, &active_request, 0)) {
							start_pend_request(active_request);
						}
					}
					else {
						// Re-start request
						start_pend_request(active_request);
					}
				}
			}
			// TODO: handle request timeouts
			continue;
		}
		// This message will come pre-verified from the protocol layer.
		//
		// Now we dispatch based on command

		switch (slots::protocol::Command(msgbuf[2])) {
			using namespace slots::protocol;

			case QUERY_TIME:
				{
					// Check to ensure we're processing this request
					if (active_request.type != PendRequest::TypeRxTime) {
						// Just dump it
						xStreamBufferReceive(dma_rx_queue, msgbuf, 9, portMAX_DELAY);
						continue;
					}

					// Otherwise, read the data into our buffer
					if (!xStreamBufferReceive(dma_rx_queue, &active_request.rx_req->status_out, 1, portMAX_DELAY)) continue;
					if (!xStreamBufferReceive(dma_rx_queue, &active_request.rx_req->timestamp_out, 8, portMAX_DELAY)) continue;

					// Notify the task that requested the time
					xTaskNotify(active_request.rx_req->notify, 1, eSetValueWithOverwrite);

					// Consume this request.
					active_request.type = PendRequest::TypeNone;
					active_request_send_retries = 0;
					// Try tail-chaining it
					if (xQueueReceive(pending_requests, &active_request, 0)) {
						start_pend_request(active_request);
					}
				}
				break;
			case ACK_DATA_TEMP:
				{
					// Handle a data temperature message
					// Read the 3 byte content
					if (!xStreamBufferReceive(dma_rx_queue, msgbuf, 3, portMAX_DELAY)) continue;
					
					uint16_t slotid = msgbuf_x16[0];

					// Update the temperature in the bheap. Never messes with any offsets so can occur w/o lock/cache clear
					if (!arena.set_temperature(slotid, msgbuf[2])) {
						ServicerLockGuard g(*this);

						arena.add_block(slotid, bheap::Block::LocationRemote, 0);
						arena.set_temperature(slotid, msgbuf[2]);

						bcache.evict();
					}

					// Check if we need to homogenize
					if (msgbuf[2] == bheap::Block::TemperatureHot && arena.get(slotid).next()) {
						ServicerLockGuard g(*this);

						arena.homogenize(slotid);
						bcache.evict();
					}

					// Check if we can process the next req
					if (active_request.type == PendRequest::TypeChangeTemp) {
						// Consume this request.
						active_request.type = PendRequest::TypeNone;
						active_request_send_retries = 0;
						// Try tail-chaining it
						if (xQueueReceive(pending_requests, &active_request, 0)) {
							start_pend_request(active_request);
						}
					}
				}
				break;
			case ACK_DATA_MOVE:
				{
					// The only current initiator of data_move is the cleanup system (for evicting blocks when the arena is too full)
					// If we're not cleaning up right now though something's gone rather wrong. We still have to process it though regardless.

					if (!xStreamBufferReceive(dma_rx_queue, msgbuf, 7, portMAX_DELAY)) continue; // This shouldn't fail due to how the dma logic works [citation needed]

					uint16_t slotid = msgbuf_x16[0],
							 start  = msgbuf_x16[1],
							 len    = msgbuf_x16[2];
					uint8_t  code   = msgbuf    [6];

					if (!code) {
						// Acquire lock
						ServicerLockGuard g(*this);

						// Evict the cache
						bcache.evict();

						// Change the location of the marked segment to remote
						arena.set_location(slotid, start, len, bheap::Block::LocationRemote);
					}

					// If we're still cleaning run the clean function again so it can send another block over (this is outside the lock
					// since we don't use recursive mutexes)
					if (is_cleaning)
						is_cleaning = do_bheap_cleanup();
				}
				break;
			case DATA_UPDATE:
			case DATA_MOVE:
				{
					uint8_t packet_length = msgbuf[1] - 8;
					uint16_t target_loc   = msgbuf[2] == DATA_UPDATE ? bheap::Block::LocationEphemeral : bheap::Block::LocationCanonical;
					// Read the header
					if (!xStreamBufferReceive(dma_rx_queue, msgbuf, 8, portMAX_DELAY)) continue;

					uint16_t sid_frame     = msgbuf_x16[0],
							 offset        = msgbuf_x16[1],
							 total_len     = msgbuf_x16[2],
							 total_upd_len = msgbuf_x16[3];

					bool start = sid_frame & (1 << 15);
					bool end   = sid_frame & (1 << 14);
					sid_frame &= 0xfff;
					
					// The process is divided into multiple steps, both for framing purposes and to save storing too much state.

					// The first message sets up most of the transfer, and is the only one that's allowed to set an error code
					if (start) {
						move_update_errcode = 0; // init back to 0 here

						// Ensure there's enough space in the buffer and decide how to setup the buffer for this new data.
						auto currsize = arena.contents_size(sid_frame);
						if (currsize == arena.npos) currsize = 0;
						if (total_len > 8400) /* impose a max limit on slot size */
							move_update_errcode = 0x3;
						else if (currsize < total_len) {
							// The slot has expanded, so we add an extra block at the end.
							// This doesn't invalidate anything due to how bheap works, so we can just do it.
							if (!arena.add_block(sid_frame, bheap::Block::LocationRemote, total_len - currsize)) {
								move_update_errcode = 0x1;

								// Mark the total clear length
								last_update_failed_delta_size = std::max<uint16_t>(last_update_failed_delta_size, (total_len - currsize) + 8);

								// Try to clean up without sending a packet to recover
								do_bheap_cleanup(false);

								// If we now have enough space (last_update_failed_delta_size is zero) re-add the block and continue along
								if (last_update_failed_delta_size == 0) {
									if (!arena.add_block(sid_frame, target_loc, total_len - currsize)) {
										move_update_errcode = 0x1;
										last_update_failed_delta_size = std::max<uint16_t>(last_update_failed_delta_size, (total_len - currsize) + 8);
										is_cleaning = do_bheap_cleanup();
									}
									else move_update_errcode = 0;
								}
								else {
									is_cleaning = do_bheap_cleanup();
								}
							}
						}
						else if (currsize > total_len) {
							// The slot has shrunk, so we truncate.
							// Similarly, this won't invalidate anything (due to the 4-byte aligned requirement)
							if (!arena.truncate_contents(sid_frame, total_len)) move_update_errcode = 0x2;
						}

						// If all that succeeded...
						if (!move_update_errcode) {
							// Ensure there's actual space for the data to go into.
							// This isn't handled during remote block adding since it's valid for the update message to grow a slot
							// without updating the new region
							if (!arena.check_location(sid_frame, offset, total_upd_len, target_loc)) {
								// Lock the heap as this messes with stuff
								ServicerLockGuard g(*this);

								// This also evicts the entire cache
								bcache.evict();
								if (!arena.set_location(sid_frame, offset, total_upd_len, target_loc)) {
									move_update_errcode = 0x1;

									// Mark the total clear length
									last_update_failed_delta_size = std::max<uint16_t>(last_update_failed_delta_size, total_upd_len + 8);

									this->give_lock();
									// Try to clean up without sending a packet to recover
									do_bheap_cleanup(false);
									this->take_lock();

									// If we now have enough space (last_update_failed_delta_size is zero) re-add the block and continue along
									if (last_update_failed_delta_size == 0) {
										if (!arena.set_location(sid_frame, offset, total_upd_len, target_loc)) {
											move_update_errcode = 0x1;
											last_update_failed_delta_size = std::max<uint16_t>(last_update_failed_delta_size, total_upd_len + 8);
											goto failed_again;
										}
										else move_update_errcode = 0;
									}
									else {
failed_again:
										this->give_lock();
										is_cleaning = do_bheap_cleanup();
										this->take_lock();
									}
								}
							}
						}
					}
					else if (!move_update_errcode) {
						// If this isn't the start message we don't do any allocation, but we do still check there is space for the data 
						// (to avoid locking it again)
						if (!arena.check_location(sid_frame, offset, packet_length, target_loc)) {
							move_update_errcode = 0x2;
						}
					}

					// Read (or discard) the remaining packet data
					amount = 0;
					while (amount < packet_length) {
						auto got = xStreamBufferReceive(dma_rx_queue, msgbuf, std::min<size_t>(16, (packet_length - amount)), portMAX_DELAY);
						// If nothing has gone wrong, update the contents in 16-byte chunks reading from the buffer.
						if (!move_update_errcode)
							if (!arena.update_contents(sid_frame, offset + amount, got, msgbuf)) move_update_errcode = 0x2;
						amount += got;
					}

					// Send an ack if we're at the end
					if (end) {
						uint16_t orig_offset = offset - (total_upd_len - packet_length);

						wait_for_not_sending();

						// Prepare the output using msgbuf
						msgbuf_x16[0] = sid_frame;
						msgbuf_x16[1] = orig_offset;
						msgbuf_x16[2] = total_upd_len;
						msgbuf    [6] = move_update_errcode;

						// Copy into the dma buffer
						memcpy(dma_out_buffer + 3, msgbuf, 7);

						// Fill out header
						dma_out_buffer[0] = 0xa5;
						dma_out_buffer[1] = 7;
						dma_out_buffer[2] = (target_loc == bheap::Block::LocationCanonical ? ACK_DATA_MOVE : ACK_DATA_UPDATE);
						
						// Send the packet
						send();

						// Don't bother waiting for it

						// If we failed due to an out of space, trigger cleanup
						if (move_update_errcode == 0x01 && !is_cleaning) {
							last_update_failed_delta_size = std::max<int16_t>(0, (int)arena.contents_size(sid_frame) - total_len);
							is_cleaning = do_bheap_cleanup();
						}

						// If this is a hot packet and not homogenous we also homogenize it
						if (arena.get(sid_frame).temperature == bheap::Block::TemperatureHot && arena.get(sid_frame).next()) {
							ServicerLockGuard g(*this);

							arena.homogenize(sid_frame);
							bcache.evict();
						}
					}
				}
				break;
			case DATA_DEL:
				{
					// Read the slot ID
					uint16_t slotid;
					if (!xStreamBufferReceive(dma_rx_queue, &slotid, 2, portMAX_DELAY)) continue;

					// Erase the contents. This doesn't screw up anything per se, but could cause dual-access problems with the cache
					{
						ServicerLockGuard g(*this);

						arena.truncate_contents(slotid, 0);
						bcache.evict(slotid);
					}

					// Send an ack
					wait_for_not_sending();

					dma_out_buffer[0] = 0xa5;
					dma_out_buffer[1] = 2;
					dma_out_buffer[2] = ACK_DATA_DEL;
					memcpy(dma_out_buffer + 3, &slotid, 2);

					send();
				}
				break;
			case slots::protocol::Command::RESET:
				NVIC_SystemReset();
			case PING:
				{
					// Send a pong
					wait_for_not_sending();

					dma_out_buffer[0] = 0xa5;
					dma_out_buffer[1] = 0;
					dma_out_buffer[2] = PONG;

					send();
				}
				break;
			default:
				// unknown command. don't bother logging it though. just eat it up

				amount = msgbuf[1];
				while (amount) {
					amount -= xStreamBufferReceive(dma_rx_queue, msgbuf, std::min<size_t>(16, amount), portMAX_DELAY);
				}

				break;
		}
	}
}

void srv::Servicer::check_connection_ping() {
	if ((timekeeper.current_time - last_comm) > 10000 && !sent_ping && !is_sending) {
		wait_for_not_sending(); // ensure this is the case although we check earlier to avoid blocking
		
		dma_out_buffer[0] = 0xa5;
		dma_out_buffer[1] = 0;
		dma_out_buffer[2] = slots::protocol::PING; // send a ping

		sent_ping = true;
		send();
	}
	if ((timekeeper.current_time - last_comm) > 30000) {
		if (timekeeper.current_time < last_comm) {
			// ignore if the clock went backwards
			last_comm = timekeeper.current_time;
			return;
		}
		// otherwise, reset
		nvic::show_error_screen("esp timeout");
	}
}

void srv::Servicer::update_forgot_statuses() {
	for (auto &block : arena) {
		if (block.location == bheap::Block::LocationRemote && block.flags & bheap::Block::FlagDirty && block.datasize) {
			wait_for_not_sending();

			dma_out_buffer[0] = 0xa5;
			dma_out_buffer[1] = 6;
			dma_out_buffer[2] = slots::protocol::DATA_FORGOT;

			*reinterpret_cast<uint16_t *>(dma_out_buffer + 3) = block.slotid;
			*reinterpret_cast<uint16_t *>(dma_out_buffer + 5) = arena.block_offset(block);
			*reinterpret_cast<uint16_t *>(dma_out_buffer + 7) = block.datasize;
			
			send();

			block.flags &= ~bheap::Block::FlagDirty;
		}
	}

	// Also mark unsent hot blocks as dirty too.
	for (auto &block : arena) {
		if (block.location == bheap::Block::LocationRemote && block.datasize && block.temperature == bheap::Block::TemperatureHot) {
			block.flags |= bheap::Block::FlagDirty;
		}
	}
}

void srv::Servicer::do_update_logic() {
	switch (this->update_state) {
		case USTATE_ERASING_BEFORE_IMAGE:
			{
				if (!READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
					CLEAR_BIT(FLASH->CR, FLASH_CR_SER);
					// Report readiness for bytes
					this->update_state = USTATE_WAITING_FOR_PACKET;
					send_update_status(slots::protocol::UpdateStatus::READY_FOR_IMAGE);
				}
				break;
			}
		case USTATE_ERASING_BEFORE_PACKET:
			{
				if (!READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
					CLEAR_BIT(FLASH->CR, FLASH_CR_SER);
					this->update_state = USTATE_WAITING_FOR_PACKET;

					append_data(this->update_state, this->update_pkg_buffer, this->update_pkg_size, true);
				}
				break;
			}
		case USTATE_PACKET_WRITTEN:
			{
				if (--this->update_chunks_remaining == 0) {
					// We are done?
					//
					// Verify checksum
				
					// Disable D-Cache and flush it
					LL_FLASH_DisableDataCache();
					LL_FLASH_EnableDataCacheReset();
					LL_FLASH_DisableDataCacheReset();

					if (util::compute_crc((uint8_t *)(0x0808'0000), this->update_total_size) != this->update_checksum) {
						this->update_state = USTATE_FAILED;
						send_update_status(slots::protocol::UpdateStatus::ABORT_CSUM);
						break;
					}

					// Checksum is OK, write the BCMD
					
					bootcmd_request_update(this->update_total_size);

					// Clear pg byte
					
					CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

					// Relock FLASH
					
					FLASH->CR |= FLASH_CR_LOCK;

					// Send out final message
					
					send_update_status(slots::protocol::UpdateStatus::BEGINNING_COPY);

					// Wait for DMA
					wait_for_not_sending();

					// Reset
					NVIC_SystemReset();

					// At this point the bootloader will update us.
				}

				this->update_state = USTATE_WAITING_FOR_PACKET;
				send_update_status(slots::protocol::UpdateStatus::READY_FOR_CHUNK);
				break;
			}
		case USTATE_PACKET_WRITE_FAIL_CSUM:
			{
				this->update_state = USTATE_WAITING_FOR_PACKET;
				send_update_status(slots::protocol::UpdateStatus::RESEND_LAST_CHUNK_CSUM);
				break;
			}
		case USTATE_WAITING_FOR_PACKET:
		case USTATE_WAITING_FOR_FINISH:
		case USTATE_WAITING_FOR_FINISH_SECTORCOUNT:
		case USTATE_WAITING_FOR_FINISH_WRITE:
		case USTATE_WAITING_FOR_READY:
			{
				// Wait for a packet
				uint8_t hdr[3];
				if (xStreamBufferReceive(dma_rx_queue, &hdr, 3, portMAX_DELAY) != 3) return;
				process_update_packet(hdr[2], hdr[1]);
			}
			break;
		default:
			vTaskDelay(10);
			break;
	}
}

void srv::Servicer::process_update_packet(uint8_t cmd, uint8_t len) {
	using namespace slots::protocol;

	switch (cmd) {
		case PING:
			{
				// ping is one of the few packets valid in both update/nonupdate mode, so we handle it here
				wait_for_not_sending();

				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0;
				dma_out_buffer[2] = PONG;

				send();
			}
			break;
		case slots::protocol::RESET:
			{
				// same with reset
				NVIC_SystemReset();
			}
		case UPDATE_CMD:
			{
				// delegate further
				UpdateCmd subcmd;
				xStreamBufferReceive(dma_rx_queue, &subcmd, 1, portMAX_DELAY);
				process_update_cmd(subcmd);
			}
			break;
		case UPDATE_IMG_START:
			{
				uint8_t msgbuf[8];
				if (len != 8) {
					send_update_status(UpdateStatus::PROTOCOL_ERROR);
					update_state = USTATE_FAILED;
					return;
				}

				xStreamBufferReceive(dma_rx_queue, msgbuf, 8, portMAX_DELAY);

				this->update_chunks_remaining = *reinterpret_cast<uint16_t *>(msgbuf + 6);
				this->update_total_size = *reinterpret_cast<uint32_t *>(msgbuf + 2);
				this->update_checksum = *reinterpret_cast<uint16_t *>(msgbuf);

				send_update_status(UpdateStatus::READY_FOR_CHUNK);
			}
			break;
		case UPDATE_IMG_DATA:
			{
				uint16_t crc;
				xStreamBufferReceive(dma_rx_queue, &crc, 2, portMAX_DELAY);

				this->update_pkg_size = len-2;
				xStreamBufferReceive(dma_rx_queue, this->update_pkg_buffer, this->update_pkg_size, portMAX_DELAY);
				memcpy(update_pkg_buffer + update_pkg_size, &crc, 2);

				append_data(update_state, update_pkg_buffer, update_pkg_size);
				break;
			}
			break;
		default:
			{
				// unhandled command in update mode
				uint8_t msgbuf[16];
				while (len) {
					len -= xStreamBufferReceive(dma_rx_queue, msgbuf, std::min<size_t>(16, len), portMAX_DELAY);
				}
				break;
			}
	}
}

bool srv::Servicer::ready() {
	return this->state >= ProtocolState::DMA_WAIT_SIZE;
}

void srv::Servicer::init() {
	// Setup HW
	this->setup_uart_dma();
	// Setup RTOS
	
	// Create the mutex for locking the bheap
	bheap_mutex = xSemaphoreCreateMutex();

	// Create the pending operation queue
	pending_requests = xQueueCreate(32, sizeof(PendRequest));

	// Create the incoming packet buffer
	dma_rx_queue = xStreamBufferCreate(2048, 3);

	// Create the debug in/out
	log_in = xStreamBufferCreate(176, 1);
	log_out = xStreamBufferCreate(128, 1); // blocking allowed

	// this_task is initialized in run()
}

void srv::Servicer::process_command() {
	// Currently this is almost exclusively going to just dump the result into the stream buffer, but for console messages it does something more intelligent
	if (dma_buffer[2] == slots::protocol::CONSOLE_MSG && dma_buffer[3] == 0x1) {
		xStreamBufferSendFromISR(log_in, dma_buffer + 4, dma_buffer[1] - 1, nullptr);
	}
	else {
		// Otherwise, we send the entire command into the queue all at once
		xStreamBufferSendFromISR(dma_rx_queue, dma_buffer, dma_buffer[1] + 3, nullptr);
	}
}

void srv::Servicer::take_lock() {
	xSemaphoreTake(bheap_mutex, portMAX_DELAY);
}

void srv::Servicer::give_lock() {
	xSemaphoreGive(bheap_mutex);
}

void srv::Servicer::start_pend_request(PendRequest req) {
	switch (req.type) {
		case PendRequest::TypeChangeTemp:
			{
				// Send a DATA_TEMP

				wait_for_not_sending();

				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 3;
				dma_out_buffer[2] = slots::protocol::DATA_TEMP;
				
				// Load the slotid

				*reinterpret_cast<uint16_t *>(dma_out_buffer + 3) = req.slotid;
				dma_out_buffer[5] = req.temperature;

				send();
			}
			break;
		case PendRequest::TypeDumpLogOut:
			{
				// Send a CONSOLE_MSG

				while (xStreamBufferBytesAvailable(log_out)) {
					wait_for_not_sending();

					dma_out_buffer[0] = 0xa5;
					dma_out_buffer[2] = slots::protocol::CONSOLE_MSG;
					dma_out_buffer[3] = 0x2;

					dma_out_buffer[1] = 1 + xStreamBufferReceive(log_out, dma_out_buffer + 4, 63, 0);

					send();
				}
			}
		case PendRequest::TypeRxTime:
			{
				wait_for_not_sending();
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x00;
				dma_out_buffer[2] = slots::protocol::QUERY_TIME;
				req.rx_req->start_out = timekeeper.current_time;
				send();
			}
		default:
			break;
	}
}

void srv::Servicer::wait_for_not_sending() {
	if (!is_sending) return;

	else {
		this->notify_on_send_done = xTaskGetCurrentTaskHandle();
		xTaskNotifyWait(0, 0xFFFF'FFFFul, nullptr, portMAX_DELAY);
	}
}

void srv::Servicer::send_update_status(slots::protocol::UpdateStatus us) {
	wait_for_not_sending();

	dma_out_buffer[0] = 0xa5;
	dma_out_buffer[1] = 0x01;
	dma_out_buffer[2] = slots::protocol::UPDATE_STATUS;
	dma_out_buffer[3] = static_cast<uint8_t>(us);

	send();
}

slots::protocol::TimeStatus srv::Servicer::request_time(uint64_t &response, uint64_t &time_when_sent) {
	slots::protocol::TimeStatus s;
	PendRequest::TimeRequest tr {
		.notify = xTaskGetCurrentTaskHandle(),
		.status_out = s,
		.timestamp_out = response,
		.start_out = time_when_sent
	};
	PendRequest pr;
	pr.type = PendRequest::TypeRxTime;
	pr.rx_req = &tr;
	xQueueSendToBack(pending_requests, &pr, portMAX_DELAY);
	// Wait for task notification
	xTaskNotifyWait(0, 0xffff'ffff, NULL, portMAX_DELAY);
	// Return status
	return s;
}

void srv::Servicer::process_update_cmd(slots::protocol::UpdateCmd cmd) {
	using namespace slots::protocol;
	switch (cmd) {
		case UpdateCmd::CANCEL_UPDATE:
			{
				// Cancel updates
				NVIC_SystemReset();
			}
		case UpdateCmd::PREPARE_FOR_IMAGE:
			{
				// Prepare to read byte
				begin_update(this->update_state);

				// When BSY bit is 0 the loop will send us to the right state.
				break;
			}
		case UpdateCmd::UPDATE_COMPLETED_OK:
			{
				// update done!
				NVIC_SystemReset();
			}
			break;
		case UpdateCmd::ESP_WROTE_SECTOR:
			{
				// sector count up
				++update_chunks_remaining;
				this->update_state = USTATE_WAITING_FOR_FINISH_SECTORCOUNT;
				break;
			}
		case UpdateCmd::ESP_COPYING:
			{
				// writing
				this->update_state = USTATE_WAITING_FOR_FINISH_WRITE;
				break;
			}
		default:
			{
				update_state = USTATE_FAILED;
			}
			break;
	}
}

const char * srv::Servicer::update_status() {
	switch (this->update_state) {
		case USTATE_WAITING_FOR_READY:
			snprintf(update_status_buffer, 16, "UPD WAIT");
			break;
		case USTATE_ERASING_BEFORE_IMAGE:
			snprintf(update_status_buffer, 16, "UPD ESIMG");
			break;
		case USTATE_FAILED:
			snprintf(update_status_buffer, 16, "UPD FAIL");
			break;
		case USTATE_PACKET_WRITE_FAIL_CSUM:
			snprintf(update_status_buffer, 16, "UPD CFAIL");
			break;
		case USTATE_ERASING_BEFORE_PACKET:
			snprintf(update_status_buffer, 16, "UPD EPS%02d", update_package_sector_counter);
			break;
		case USTATE_WAITING_FOR_PACKET:
			snprintf(update_status_buffer, 16, "UPD P%04d", update_chunks_remaining);
			break;
		case USTATE_WAITING_FOR_FINISH:
			snprintf(update_status_buffer, 16, "UPD WESP");
			break;
		case USTATE_WAITING_FOR_FINISH_SECTORCOUNT:
			snprintf(update_status_buffer, 16, "UPD ESCT%04d", update_chunks_remaining);
			break;
		case USTATE_WAITING_FOR_FINISH_WRITE:
			snprintf(update_status_buffer, 16, "UPD EWRT");
		default:
			break;
	}

	return update_status_buffer;
}
// override _write

extern "C" int __attribute__((used)) _write(int file, char* ptr, int len) {
	return 0; // todo
}
