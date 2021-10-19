#include "srv.h"

#include "common/slots.h"
#include "crash/main.h"
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
	if (!needs_temperature_update(slotid, temp)) return;
	PendRequest pr;
	pr.type = PendRequest::TypeChangeTemp;
	pr.slotid = slotid;
	pr.temperature = temp;
	xQueueSendToBack(pending_requests, &pr, pdMS_TO_TICKS(2000));
}

bool srv::Servicer::needs_temperature_update(uint16_t slotid, uint16_t temp) {
	if (std::as_const(arena).get(slotid) && std::as_const(arena).get(slotid).temperature == temp) return false;
	else if (!std::as_const(arena).get(slotid) && temp == bheap::Block::TemperatureCold) return false;
	return true;
}

void srv::Servicer::refresh_grabber(slots::protocol::GrabberID gid) {
	PendRequest pr;
	pr.type = PendRequest::TypeRefreshGrabber;
	pr.refresh = gid;
	xQueueSendToBack(pending_requests, &pr, pdMS_TO_TICKS(2000));
	immediately_process();
}

void srv::Servicer::set_sleep_mode(bool enabled) {
	PendRequest pr;
	pr.type = PendRequest::TypeSleepMode;
	pr.sleeping = enabled;
	xQueueSendToBack(pending_requests, &pr, pdMS_TO_TICKS(2000));
	immediately_process();
}

void srv::Servicer::reset() {
	PendRequest pr;
	pr.type = PendRequest::TypeReset;
	xQueueSendToBack(pending_requests, &pr, portMAX_DELAY);
	while (1) {vTaskDelay(portMAX_DELAY);}
}

void srv::Servicer::set_temperature_group(uint32_t temperature, uint16_t amt, const uint16_t * sids, bool sync) {
	PendRequest pr;
	pr.type = PendRequest::TypeChangeTempMulti;
	pr.mt_req.temperature = temperature;
	pr.mt_req.amount = amt;
	pr.mt_req.entries = sids;
	xQueueSendToBack(pending_requests, &pr, portMAX_DELAY);
	if (sync) {
		pr.type = PendRequest::TypeSync;
		pr.sync_with = xTaskGetCurrentTaskHandle();
		xQueueSendToBack(pending_requests, &pr, portMAX_DELAY);
		immediately_process();
		xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
	}
	else {
		immediately_process();
	}
}

bool srv::Servicer::slot_dirty(uint16_t slotid, bool clear) {
	bool is_dirty = std::any_of(arena.cbegin(slotid), arena.cend(slotid), [](const auto& b){return b.flags & bheap::Block::FlagDirty;});
	if (is_dirty && clear) {
		for (auto it = arena.begin(slotid); it != arena.end(slotid); ++it) {
			it->flags &= ~bheap::Block::FlagDirty;
		}
	}

	return is_dirty;
}


const bheap::Block& srv::Servicer::_slot(uint16_t slotid) {
	return arena.get(slotid);
}

void srv::Servicer::try_cleanup_heap(ssize_t need_space) {
	// Do periodic/required cleanup:
	//
	// This function returns true if it sent a packet.
	
	// Do a defrag regardless to calculate free space correctly
	
	srv::ServicerLockGuard g(*this); // Lock the buffer
	arena.defrag();
	
	// Start by checking how much free allocatable space we have
	
	auto current_space = arena.free_space(); // buffer region
	if (current_space < 32) current_space = 0;
	else current_space -= 32;

	// TODO: use high_watermark_complex when checking for defrags
	int to_free = need_space - current_space;
	if (to_free < 0) return;

	// Otherwise we begin freeing up blocks.
	// This first pass will eliminate all non-cached cold ephemeral blocks
	//
	// We free from left to right; this could probably be more efficient going right to left but it'd use more memory more of the time.
	for (int stage = 0; stage < 8; ++stage) {
repeatloop:
		for (auto& block : arena) {
			if (block && block.location == bheap::Block::LocationEphemeral && block.datasize 
				&& block.temperature == 
				   (stage & 4 ? bheap::Block::TemperatureWarm : bheap::Block::TemperatureCold) // then finally warm ones.
				&& (stage & 2 || !arena.cached(block.slotid))   // then cached ones
				&& (stage & 1 || block.datasize >= to_free)) { // try and free big blocks first

				// Delete this block (shrink to size = 0 to create empty set location to remote and reset datasize)
				auto len = block.datasize;

				// Only free up so much
				if (len >= (to_free + 20)) len = to_free;

				// Send out a packet with the freed block immediately, so the ESP knows to send it.
				wait_for_not_sending();

				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 6;
				dma_out_buffer[2] = slots::protocol::DATA_REQUEST;

				*reinterpret_cast<uint16_t *>(dma_out_buffer + 3) = block.slotid;
				*reinterpret_cast<uint16_t *>(dma_out_buffer + 5) = arena.block_offset(block);
				*reinterpret_cast<uint16_t *>(dma_out_buffer + 7) = block.datasize;
				
				send();
				// Set location afterwards to avoid invalidating block
				arena.set_location(block.slotid, arena.block_offset(block), len, bheap::Block::LocationRemote);

				auto freed = len + (len % 4 ? 0 : 4 - len % 4);
				to_free -= freed;
				current_space += freed;
				if (to_free <= 0) {
					return;
				}
				else goto repeatloop; // avoid invalid iteration
			}
		}

		// We now do a cleanup to try and reclaim space if it'll recover enough on its own
		if (arena.free_space(arena.FreeSpaceDefrag) - current_space >= to_free) {
			arena.defrag();

			return;
		}
	}

	arena.defrag();
}

void srv::Servicer::do_handshake() {
	int retries = 0;

	// Check if the system has just updated
	if (bootcmd_did_just_update()) {
		// We did! Change the baudrate since the ESP is still waiting at 921600.
		switch_fast();
		// Send out an update state finished
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
				// Before entering the update mode, switch baudrate
				switch_fast();
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
	slots::protocol::DataStoreFulfillResult move_update_errcode = slots::protocol::DataStoreFulfillResult::Ok;
	uint16_t last_slotid = 0xfff; uint8_t last_slotid_tries = 0;
	uint8_t active_request_statemachine = 0;

	auto end_active_request = [&]() {
poll_another_pr:
		// End request
		active_request_send_retries = 0;
		// Consume this request.
		active_request.type = PendRequest::TypeNone;
		// Try tail-chaining it
		if (xQueueReceive(pending_requests, &active_request, 0)) {
			if (start_pend_request(active_request)) goto poll_another_pr;
		}
	};

	size_t hdramount = 0, amount = 0;

	// Otherwise, begin processing packets/requests
	while (true) {
		// End active request if it doesn't have any loop actions (in theory this should never happen, but useful to keep around anyways)
		switch (active_request.type) {
			default: break;
			case PendRequest::TypeRefreshGrabber:
			case PendRequest::TypeSleepMode:
			case PendRequest::TypeDumpLogOut:
			case PendRequest::TypeSync:
				end_active_request();
				break;
		}

		// If no active request is pending, check if any requests were made while processing a packet
		if (active_request.type == PendRequest::TypeNone) {
			end_active_request(); // perhaps a misnomer but will restart as well.
		}

		// Wait for a packet to come in over DMA. If this returns 0 (i.e. no packet but we still interrupted) then check the queue for a new task.
		//
		// This loop is effectively managing two separate tasks, one static - normal packets - and one that changes based on the top of the queue.
		// Additionally, a queue event will only ever cause a write and "wait" for a read in response.

		can_interrupt_with_notification = true;
		hdramount += xStreamBufferReceive(dma_rx_queue, msgbuf + hdramount, 3 - hdramount, pdMS_TO_TICKS(400)); // every 100 ms we also try and look for cleanup-problems
		can_interrupt_with_notification = false;
		// Check for a new queue operation if we're not still processing one.
		if (hdramount < 3) {
			if (active_request.type == PendRequest::TypeNone) {
poll_another_pr:
				// Check for new queue operation
				if (xQueueReceive(pending_requests, &active_request, 0)) {
					// Start handling it.
					active_request_send_retries = 0;
					if (start_pend_request(active_request)) {
						active_request_send_retries = 0;
						active_request.type = PendRequest::TypeNone;
						goto poll_another_pr;
					}
				}
				// Otherwise, do various random tasks.
				else {
					check_connection_ping();
					send_data_requests();

					if (xStreamBufferBytesAvailable(log_out) > 100) {
						// Do a flush
						active_request_send_retries = 0;
						start_pend_request(PendRequest{
							.type = PendRequest::TypeDumpLogOut
						});
					}
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
						end_active_request();
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
		hdramount = 0;
		// This message will come pre-verified from the protocol layer, but we'll check anyways in case something overran
		if (msgbuf[0] != slots::PacketWrapper<>::FromEsp) continue;
		//
		// Now we dispatch based on command

		switch (slots::protocol::Command(msgbuf[2])) {
			using namespace slots::protocol;

			case QUERY_TIME:
				{
					// Check to ensure we're processing this request
					if (active_request.type != PendRequest::TypeRxTime || !*active_request.rx_req) {
						// Just dump it
						xStreamBufferReceive(dma_rx_queue, msgbuf, 9, portMAX_DELAY);
						continue;
					}

					// Otherwise, read the data into our buffer
					if (!xStreamBufferReceive(dma_rx_queue, &active_request.rx_req->status_out, 1, portMAX_DELAY)) continue;
					if (!xStreamBufferReceive(dma_rx_queue, &active_request.rx_req->timestamp_out, 8, portMAX_DELAY)) continue;

					// Notify the task that requested the time
					xTaskNotify(active_request.rx_req->notify, 1, eSetValueWithOverwrite);

					// Finish query
					end_active_request();
				}
				break;
			case ACK_DATA_TEMP:
				{
					// Handle a data temperature message
					// Read the 3 byte content
					if (!xStreamBufferReceive(dma_rx_queue, msgbuf, 3, portMAX_DELAY)) continue;
					
					uint16_t slotid = msgbuf_x16[0];
					// Is this an ack?
					if (msgbuf[2] != 0xff) {
						ServicerLockGuard g(*this);

						// Update the temperature in the bheap. Never messes with any offsets so can occur w/o lock/cache clear
						if (!arena.set_temperature(slotid, msgbuf[2])) {
							arena.add_block(slotid, bheap::Block::LocationRemote, 0);
							arena.set_temperature(slotid, msgbuf[2]);
						}

						// Check if we need to homogenize
						if (msgbuf[2] == bheap::Block::TemperatureHot && arena.get(slotid).next()) {
							arena.homogenize(slotid);
						}
					}
					// Check if we can process the next req
					if (active_request.type == PendRequest::TypeChangeTemp) {
end_temp_request:
						// Consume this request.
						end_active_request();
					}
					else if (active_request.type == PendRequest::TypeChangeTempMulti) {
						// Are there more entries left?
						active_request.mt_req.amount--;
						active_request.mt_req.entries++;
						if (active_request.mt_req.amount) {
							if (start_pend_request(active_request)) end_active_request();
						}
						else {
							goto end_temp_request;
						}
					}
				}
				break;
			case DATA_TEMP:
				{
					// Change temperature.
					if (!xStreamBufferReceive(dma_rx_queue, msgbuf, 3, portMAX_DELAY)) continue;
					uint16_t slotid = msgbuf_x16[0];

					wait_for_not_sending();

					dma_out_pkt.direction = dma_out_pkt.FromStm;
					dma_out_pkt.cmd_byte =  slots::protocol::ACK_DATA_TEMP;
					dma_out_pkt.size = 3;
					dma_out_pkt.put(slotid, 0);

					{
						ServicerLockGuard g(*this);
						if (arena.set_temperature(slotid, msgbuf[2])) dma_out_pkt.data()[2] = msgbuf[2];
						else dma_out_pkt.data()[2] = 0xff;
					}

					send();
				}
				break;
			case ACK_DATA_STORE:
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

						arena.set_location(slotid, start, len, bheap::Block::LocationRemote);
					}
				}
				break;
			case DATA_RETRIEVE:
				{
					if (!xStreamBufferReceive(dma_rx_queue, msgbuf, 6, portMAX_DELAY)) continue; // This shouldn't fail due to how the dma logic works [citation needed]

					uint16_t slotid = msgbuf_x16[0],
							 start  = msgbuf_x16[1],
							 len    = msgbuf_x16[2];

					ServicerLockGuard g(*this);
					
					// Ensure the entire block is actually stored here
					if (!arena.check_location(slotid, start, len, bheap::Block::LocationCanonical)) break; // ignore packet

					wait_for_not_sending();

					// Send an ack first of all
					dma_out_pkt.direction = dma_out_pkt.FromStm;
					dma_out_pkt.cmd_byte =  slots::protocol::ACK_DATA_RETRIEVE;
					dma_out_pkt.size = 6;
					dma_out_pkt.put(slotid, 0);
					dma_out_pkt.put(start, 2);
					dma_out_pkt.put(len, 4);

					send();

					uint16_t sendoffset = start;

					// NOTE: our blocks might not be the same as the ESP's blocks, so we have to ensure we only send the blocks it wants.
					for (auto *b = &arena.get(slotid, start); b && arena.block_offset(*b) < (start + len); b = b->next()){
						if (!*b || !b->datasize) continue;
						uint8_t total_size = 64 - 6;
						for (
							uint16_t suboffset = (sendoffset - arena.block_offset(*b));
							suboffset < b->datasize && sendoffset < (start + len);
							suboffset += total_size, sendoffset += total_size
						) {
							// Calculate total size
							total_size = std::min<ssize_t>(total_size, (start + len) - sendoffset);
							total_size = std::min<ssize_t>(total_size, b->datasize - suboffset);

							//if (!total_size) break;

							bool is_start = sendoffset == start;
							bool is_end   = (sendoffset + total_size) == (start + len);

							// Prepare packet
							wait_for_not_sending();
							dma_out_pkt.direction = dma_out_pkt.FromStm;
							dma_out_pkt.cmd_byte =  slots::protocol::DATA_STORE;
							dma_out_pkt.size = total_size + 6;
							dma_out_pkt.put<uint16_t>(slotid | (is_start << 15) | (is_end << 14), 0);
							dma_out_pkt.put(sendoffset, 2);
							dma_out_pkt.put(len, 4);
							memcpy(dma_out_pkt.data() + 6, (uint8_t *)b->data() + suboffset, total_size);
							send();
						}
					}
				}
				break;
			case DATA_FULFILL:
			case DATA_STORE:
				{
					uint8_t packet_length = msgbuf[1] - 6;
					uint16_t target_loc   = msgbuf[2] == DATA_FULFILL ? bheap::Block::LocationEphemeral : bheap::Block::LocationCanonical;
					// Read the header
					if (!xStreamBufferReceive(dma_rx_queue, msgbuf, 6, portMAX_DELAY)) continue;

					uint16_t sid_frame     = msgbuf_x16[0],
							 offset        = msgbuf_x16[1],
							 total_upd_len = msgbuf_x16[2];

					bool start = sid_frame & (1 << 15);
					bool end   = sid_frame & (1 << 14);
					sid_frame &= 0xfff;

					{
						// Always lock the heap (caching issues)
						ServicerLockGuard g(*this);

						// The process is divided into multiple steps, both for framing purposes and to save storing too much state.

						// The first message sets up most of the transfer, and is the only one that's allowed to set an error code
						if (start) {
							move_update_errcode = slots::protocol::DataStoreFulfillResult::Ok; // init back to 0 here

							// Keep track of retries
							if (last_slotid == sid_frame) ++last_slotid_tries;
							else {
								last_slotid = sid_frame;
								last_slotid_tries = 0;
							}

							// Ensure there's enough space in the buffer and decide how to setup the buffer for this new data.
							auto currsize = arena.contents_size(sid_frame);
							if (offset + total_upd_len > currsize) {
								move_update_errcode = slots::protocol::DataStoreFulfillResult::InvalidOrNak;
							}
							// If all that succeeded...
							else {
								// Ensure there's actual space for the data to go into.
								// This isn't handled during remote block adding since it's valid for the update message to grow a slot
								// without updating the new region
								if (!arena.check_location(sid_frame, offset, total_upd_len, target_loc)) {
									if (!arena.set_location(sid_frame, offset, total_upd_len, target_loc)) {
										move_update_errcode = slots::protocol::DataStoreFulfillResult::NotEnoughSpace_TryAgain;

										// Compute how much new space is needed
										ssize_t current_allocated_space = 0;
										for (auto *b = &arena.get(sid_frame, offset); b && arena.block_offset(*b) < (offset + total_upd_len); b = b->next()) {
											if (b->location != bheap::Block::LocationRemote) {
												auto inner_off = arena.block_offset(*b);
												auto inner_begin = std::max<size_t>(inner_off, offset);
												auto inner_end   = std::min<size_t>(offset + total_upd_len, inner_off + b->datasize);
												current_allocated_space += (inner_end - inner_begin);
											}
											else {
												current_allocated_space -= 4;
											}
										}

										// Mark the total clear length
										this->give_lock();
										// Try to clean up without sending a packet to recover
										try_cleanup_heap(last_slotid_tries > 1 ? 8 + total_upd_len : 4 + (total_upd_len - current_allocated_space));
										this->take_lock();

										if (last_slotid_tries > 2) {
											move_update_errcode = slots::protocol::DataStoreFulfillResult::NotEnoughSpace_Failed;
										}
									}
								}
							}
						}
						else if (move_update_errcode == slots::protocol::DataStoreFulfillResult::Ok) {
							// If this isn't the start message we don't do any allocation, but we do still check there is space for the data 
							// (to avoid locking it again)
							if (!arena.check_location(sid_frame, offset, packet_length, target_loc)) {
								move_update_errcode = slots::protocol::DataStoreFulfillResult::IllegalState;
							}
						}

						// Read (or discard) the remaining packet data
						amount = 0;
						while (amount < packet_length) {
							auto got = xStreamBufferReceive(dma_rx_queue, msgbuf, std::min<size_t>(16, (packet_length - amount)), pdMS_TO_TICKS(5));
							// If nothing has gone wrong, update the contents in 16-byte chunks reading from the buffer.
							if (move_update_errcode == slots::protocol::DataStoreFulfillResult::Ok)
								if (!arena.update_contents(sid_frame, offset + amount, got, msgbuf)) move_update_errcode = slots::protocol::DataStoreFulfillResult::IllegalState;
							amount += got;
							if (got == 0) {
								// Timed out waiting, just give up -- the ESP was probably sending packets too fast.
								break;
							}
						}

						// If this is a warm packet we homogenize too
						if (arena.get(sid_frame).temperature >= bheap::Block::TemperatureWarm && arena.get(sid_frame).next()) {
							arena.homogenize(sid_frame);
						}
					}

					// Send an ack if we're at the end
					if (end) {
						uint16_t orig_offset = offset - (total_upd_len - packet_length);

						wait_for_not_sending();

						// Fill out header
						dma_out_buffer[0] = 0xa5;
						dma_out_buffer[1] = 7;
						dma_out_buffer[2] = (target_loc == bheap::Block::LocationCanonical ? ACK_DATA_STORE : ACK_DATA_FULFILL);

						dma_out_pkt.put<uint16_t>(sid_frame, 0);
						dma_out_pkt.put<uint16_t>(orig_offset, 2);
						dma_out_pkt.put<uint16_t>(total_upd_len, 4);
						dma_out_pkt.data()[6] = (uint8_t)move_update_errcode;
						
						// Send the packet
						send();

						// If the errcode was 0, reset retries
						if (move_update_errcode == slots::protocol::DataStoreFulfillResult::Ok) last_slotid_tries = 0;
					}
				}
				break;
			case DATA_SET_SIZE:
				{
					// Read the slot ID
					if (!xStreamBufferReceive(dma_rx_queue, &msgbuf, 4, portMAX_DELAY)) continue;

					{
						ServicerLockGuard g(*this);

						auto currsize = arena.contents_size(msgbuf_x16[0]);
						if (currsize != msgbuf_x16[1]) {
							if (currsize > msgbuf_x16[1]) {
								arena.truncate_contents(msgbuf_x16[0], msgbuf_x16[1]);
							}
							else {
								// Add an empty remote block
								if (!arena.add_block(msgbuf_x16[0], bheap::Block::LocationRemote, msgbuf_x16[1] - currsize)) {
									this->give_lock();
									try_cleanup_heap(4);
									this->take_lock();
									if (!arena.add_block(msgbuf_x16[0], bheap::Block::LocationRemote, msgbuf_x16[1] - currsize)) break; // just give up
								}
							}
						}

						// Homogenize all new blocks for hot slots
						if (auto &first = arena.get(msgbuf_x16[0]); first.temperature >= bheap::Block::TemperatureWarm && first.next()) arena.homogenize(msgbuf_x16[0]);
					}

					// Send an ack
					wait_for_not_sending();

					dma_out_buffer[0] = 0xa5;
					dma_out_buffer[1] = 4;
					dma_out_buffer[2] = ACK_DATA_SET_SIZE;
					dma_out_pkt.put(msgbuf_x16[0], 0);
					dma_out_pkt.put(msgbuf_x16[1], 2);

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
#ifndef SIM
	if ((timekeeper.current_time - last_comm) > 30000) {
		if (timekeeper.current_time < last_comm) {
			// ignore if the clock went backwards
			last_comm = timekeeper.current_time;
			return;
		}
		// otherwise, reset
		crash::panic_nonfatal("esp timeout");
	}
#endif
}

void srv::Servicer::send_data_requests() {
	ServicerLockGuard g(*this);

	for (auto &block : arena) {
		if (block && block.location == bheap::Block::LocationRemote && block.flags & bheap::Block::FlagDirty && block.datasize) {
			wait_for_not_sending();

			dma_out_buffer[0] = 0xa5;
			dma_out_buffer[1] = 6;
			dma_out_buffer[2] = slots::protocol::DATA_REQUEST;

			*reinterpret_cast<uint16_t *>(dma_out_buffer + 3) = block.slotid;
			*reinterpret_cast<uint16_t *>(dma_out_buffer + 5) = arena.block_offset(block);
			*reinterpret_cast<uint16_t *>(dma_out_buffer + 7) = block.datasize;
			
			send();

			block.flags &= ~bheap::Block::FlagDirty;
		}
	}

	// Also mark unsent hot blocks as dirty too.
	for (auto &block : arena) {
		if (block && block.location == bheap::Block::LocationRemote && block.datasize && block.temperature >= bheap::Block::TemperatureWarm) {
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
	bheap_mutex = xSemaphoreCreateMutexStatic(&bheap_mutex_private);

	// Create the pending operation queue
	pending_requests = xQueueCreateStatic(sizeof pending_requests_data / sizeof(PendRequest), sizeof(PendRequest), pending_requests_data, &pending_requests_private);

	// Create the incoming packet buffer
	dma_rx_queue = xStreamBufferCreateStatic(sizeof dma_rx_queue_data, 3, dma_rx_queue_data, &dma_rx_queue_private);

	// Create the debug in/out
	log_in = xStreamBufferCreateStatic(sizeof log_in_data, 1, log_in_data, &log_in_private);
	log_out = xStreamBufferCreateStatic(sizeof log_out_data, 1, log_out_data, &log_out_private); // blocking allowed

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

bool srv::Servicer::start_pend_request(PendRequest req) {
	switch (req.type) {
		case PendRequest::TypeChangeTemp:
			{
				if (!needs_temperature_update(req.slotid, req.temperature)) return true;
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
		case PendRequest::TypeChangeTempMulti:
			{
				while (req.mt_req.amount && !needs_temperature_update(req.mt_req.entries[0], req.mt_req.temperature)) {
					req.mt_req.amount--;
					req.mt_req.entries++;
				}

				if (req.mt_req.amount == 0) return true;
				// Send the next entry.

				wait_for_not_sending();

				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 3;
				dma_out_buffer[2] = slots::protocol::DATA_TEMP;
				
				// Load the slotid
				dma_out_pkt.put(req.mt_req.entries[0], 0);
				dma_out_buffer[5] = req.mt_req.temperature;

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

				return true;
			}
			break;
		case PendRequest::TypeRxTime:
			{
				if (!*req.rx_req) return true;
				wait_for_not_sending();
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x00;
				dma_out_buffer[2] = slots::protocol::QUERY_TIME;
				req.rx_req->start_out = timekeeper.current_time;
				send();
			}
			break;
		case PendRequest::TypeRefreshGrabber:
			{
				wait_for_not_sending();
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x01;
				dma_out_buffer[2] = slots::protocol::REFRESH_GRABBER;
				dma_out_buffer[3] = (uint8_t)req.refresh;
				send();

				return true;
			}
			break;
		case PendRequest::TypeSleepMode:
			{
				wait_for_not_sending();
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x01;
				dma_out_buffer[2] = slots::protocol::SLEEP_ENABLE;
				dma_out_buffer[3] = (uint8_t)req.sleeping;
				send();
				bootcmd_set_silent(req.sleeping);

				return true;
			}
			break;
		case PendRequest::TypeReset:
			{
				// send message, delay, then reset
				wait_for_not_sending();
				dma_out_buffer[0] = 0xa5;
				dma_out_buffer[1] = 0x00;
				dma_out_buffer[2] = slots::protocol::RESET;
				send();
				wait_for_not_sending();
				NVIC_SystemReset();
			}
		case PendRequest::TypeSync:
			{
				xTaskNotify(req.sync_with, 0xffff'ffff, eSetValueWithOverwrite);
				return true;
			}
		default:
			return true;
	}
	return false;
}

void srv::Servicer::wait_for_not_sending() {
	if (!is_sending) return;

	else {
		this->notify_on_send_done = xTaskGetCurrentTaskHandle();
		while (is_sending) xTaskNotifyWait(0, 0xFFFF'FFFFul, nullptr, pdMS_TO_TICKS(50));
		this->notify_on_send_done = nullptr;
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
	slots::protocol::TimeStatus s = slots::protocol::TimeStatus::Timeout;
	PendRequest::TimeRequest tr {
		.notify = xTaskGetCurrentTaskHandle(),
		.status_out = s,
		.timestamp_out = response,
		.start_out = time_when_sent
	};
	PendRequest pr;
	pr.type = PendRequest::TypeRxTime;
	pr.rx_req = &tr;
	if (xQueueSendToBack(pending_requests, &pr, pdMS_TO_TICKS(5000)) != pdPASS) return s; // we only send to the queue for time requests since they aren't particularly important.
	// Wait for task notification
	xTaskNotifyWait(0, 0xffff'ffff, NULL, pdMS_TO_TICKS(10000));
	// Return status
	return s;
}

void srv::Servicer::immediately_process() {
	if (this_task && can_interrupt_with_notification) xTaskNotify(this_task, 0xffff0000, eSetValueWithoutOverwrite);
	// if can_interrupt_with_notification is false, the main task is going to check for new data _anyways_
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
